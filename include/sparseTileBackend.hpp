#pragma once

#include <glad/glad.h>

#include <cstdlib>
#include <glm/glm.hpp>
#include <vector>

#include "boolOps.hpp"       // VoxelObject, VoxelizationParams
#include "carveBackend.hpp"  // ICarveBackend
#include "shader.hpp"

// =============================================================================
//  SparseTileBackend — carving on a TILED workpiece layout (idea (c) of the
//  optimization survey; DOCS/DEV_PLAN/sparse-tile-carve-plan.md).
//
//  STAGE 1 (correctness-first): the stock is stored per TILE (TS×TS columns per
//  tile, K slots per column) but EVERY tile is materialized — i.e. this is a
//  reordering of the flat buffer, not yet a memory saving. It exists to prove the
//  tiled addressing (shared with subtract_swept.comp's `tiled` path), the carve,
//  and the row-major result assembly are BIT-EXACT vs the flat backend. The
//  UNIFORM-tile fast path (the actual working-set reduction) is Stage 2.
//
//  Removed-voxel tracking (fitness) is not supported here yet; countRemoved = 0.
// =============================================================================
class SparseTileBackend : public ICarveBackend {
 public:
  SparseTileBackend() { shader_ = new Shader("shaders/subtract_swept.comp"); }
  ~SparseTileBackend() override {
    delete shader_;
    for (GLuint* b : {&poolBuf_, &countBuf_, &toolComp_, &toolPref_, &removedBuf_})
      if (*b) glDeleteBuffers(1, b);
  }
  const char* name() const override { return "sparse"; }

  void prepare(const VoxelObject& stock, const VoxelObject& tool) override {
    params_ = stock.params;
    W_ = stock.params.resolutionXYZ.x;  H_ = stock.params.resolutionXYZ.y;  Z_ = stock.params.resolutionXYZ.z;
    w2_ = tool.params.resolutionXYZ.x;  h2_ = tool.params.resolutionXYZ.y;  z2_ = tool.params.resolutionXYZ.z;
    nX_ = (W_ + TS_ - 1) / TS_;  nY_ = (H_ + TS_ - 1) / TS_;

    const long nTiles = (long)nX_ * nY_;
    const long colsPerTile = (long)TS_ * TS_;

    // CPU-build the tiled pool + per-column counts from the compressed stock.
    std::vector<GLuint> pool((size_t)nTiles * colsPerTile * K_, 0u);
    std::vector<GLuint> counts((size_t)nTiles * colsPerTile, 0u);
    const auto& comp = stock.compressedData;
    const auto& pref = stock.prefixSumData;
    const size_t nCols = pref.size();  // == W*H
    for (int gy = 0; gy < H_; ++gy)
      for (int gx = 0; gx < W_; ++gx) {
        const size_t idx = (size_t)gy * W_ + gx;
        const uint32_t s = pref[idx];
        const uint32_t e = (idx + 1 < nCols) ? pref[idx + 1] : (uint32_t)comp.size();
        const long cidx = tileColumn(gx, gy);
        counts[(size_t)cidx] = e - s;
        const long base = cidx * K_;
        for (uint32_t t = s; t < e && (long)(t - s) < K_; ++t) pool[(size_t)base + (t - s)] = comp[t];
      }

    upload(poolBuf_, pool);
    upload(countBuf_, counts);
    upload(toolComp_, tool.compressedData);
    upload(toolPref_, tool.prefixSumData);
    upload(removedBuf_, std::vector<GLuint>(1, 0u));  // dummy (countRemoved = 0)
  }

  bool carveSwept(const glm::ivec3& startOffset, const glm::ivec3& displacement, int /*segmentIndex*/) override {
    const int w1 = W_, h1 = H_, z1 = Z_, w2 = w2_, h2 = h2_, z2 = z2_;
    // Same host math as BoolOps::subtractSwept (inverted-Z convention, Chebyshev K).
    glm::ivec3 endOffset = startOffset + displacement;
    glm::ivec3 tStart(w1 / 2 + startOffset.x, h1 / 2 + startOffset.y, z1 / 2 - startOffset.z);
    glm::ivec3 tEnd(w1 / 2 + endOffset.x, h1 / 2 + endOffset.y, z1 / 2 - endOffset.z);
    glm::ivec3 tDelta = tEnd - tStart;
    glm::ivec3 ad = glm::abs(tDelta);
    int K = glm::max(glm::max(ad.x, ad.y), ad.z);

    // #8: whole-segment in-air skip (parity with the flat backend).
    long zA = tStart.z < tEnd.z ? tStart.z : tEnd.z, zB = tStart.z > tEnd.z ? tStart.z : tEnd.z;
    if (zB + z2 / 2 <= 0 || zA - z2 / 2 >= z1) return true;

    long minTx = glm::min(tStart.x, tEnd.x), maxTx = glm::max(tStart.x, tEnd.x);
    long minTy = glm::min(tStart.y, tEnd.y), maxTy = glm::max(tStart.y, tEnd.y);
    long baseX = glm::clamp(minTx - w2 / 2, 0L, (long)w1), endX = glm::clamp(maxTx + w2 / 2, 0L, (long)w1);
    long baseY = glm::clamp(minTy - h2 / 2, 0L, (long)h1), endY = glm::clamp(maxTy + h2 / 2, 0L, (long)h1);
    if (endX <= baseX || endY <= baseY) return true;

    shader_->use();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, poolBuf_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, countBuf_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, toolComp_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, toolPref_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, removedBuf_);
    shader_->setInt("w1", w1);  shader_->setInt("h1", h1);  shader_->setInt("z1", z1);
    shader_->setInt("w2", w2);  shader_->setInt("h2", h2);  shader_->setInt("z2", z2);
    shader_->setUInt("maxTransitions", (unsigned)K_);
    shader_->setInt("baseX", (int)baseX);  shader_->setInt("baseY", (int)baseY);
    shader_->setIVec3("translateStart", tStart);
    shader_->setIVec3("translateDelta", tDelta);
    shader_->setInt("numSubsteps", K);
    shader_->setInt("countRemoved", 0);  shader_->setInt("segmentIndex", 0);
    shader_->setInt("enableSkip", sweptSkip());
    shader_->setInt("tiled", 1);  shader_->setInt("tileTS", TS_);  shader_->setInt("tileNX", nX_);

    GLuint gX = (GLuint)((endX - baseX + 7) / 8), gY = (GLuint)((endY - baseY + 7) / 8);
    glDispatchCompute(gX, gY, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    return true;
  }

  void finish() override { glFinish(); }

  void readResult(VoxelObject& out) override {
    const long nTiles = (long)nX_ * nY_, colsPerTile = (long)TS_ * TS_;
    std::vector<GLuint> pool = download(poolBuf_, (size_t)nTiles * colsPerTile * K_);
    std::vector<GLuint> counts = download(countBuf_, (size_t)nTiles * colsPerTile);
    out.params = params_;
    out.compressedData.clear();
    out.prefixSumData.assign((size_t)W_ * H_, 0u);
    for (int gy = 0; gy < H_; ++gy)
      for (int gx = 0; gx < W_; ++gx) {
        const size_t idx = (size_t)gy * W_ + gx;
        out.prefixSumData[idx] = (GLuint)out.compressedData.size();
        const long cidx = tileColumn(gx, gy), base = cidx * K_;
        const uint32_t cnt = counts[(size_t)cidx];
        for (uint32_t t = 0; t < cnt; ++t) out.compressedData.push_back(pool[(size_t)base + t]);
      }
  }

 private:
  // Global column (gx,gy) -> flat index into the tiled counts array (and, ×K, the pool).
  long tileColumn(int gx, int gy) const {
    const long tx = gx / TS_, ty = gy / TS_;
    const long lcol = (gx - tx * TS_) + (gy - ty * TS_) * (long)TS_;
    return (tx + ty * (long)nX_) * (long)TS_ * TS_ + lcol;
  }
  static int sweptSkip() {
    static const int s = [] { const char* e = std::getenv("AUTOCAM_SWEPT_SKIP"); return (e && e[0] == '0') ? 0 : 1; }();
    return s;
  }
  void upload(GLuint& buf, const std::vector<GLuint>& d) {
    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(d.size() * sizeof(GLuint)), d.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  }
  std::vector<GLuint> download(GLuint buf, size_t n) {
    std::vector<GLuint> d(n);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(n * sizeof(GLuint)), d.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return d;
  }

  Shader* shader_ = nullptr;
  VoxelizationParams params_{};
  int W_ = 0, H_ = 0, Z_ = 0, w2_ = 0, h2_ = 0, z2_ = 0;
  static constexpr int TS_ = 32;  // tile side (columns)
  static constexpr int K_ = 32;   // per-column slots (Stage 1: same as flat MAX_TRANSITIONS)
  int nX_ = 0, nY_ = 0;
  GLuint poolBuf_ = 0, countBuf_ = 0, toolComp_ = 0, toolPref_ = 0, removedBuf_ = 0;
};
