#pragma once

#include <glad/glad.h>

#include <algorithm>
#include <cstdlib>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

#include "boolOps.hpp"       // VoxelObject, VoxelizationParams
#include "carveBackend.hpp"  // ICarveBackend
#include "shader.hpp"

// =============================================================================
//  SparseTileBackend — carving on a TILED workpiece layout with a UNIFORM fast
//  path (idea (c); DOCS/DEV_PLAN/sparse-tile-carve-plan.md).
//
//  STAGE 2: the stock is partitioned into TS×TS-column tiles. A tile whose columns
//  are all the full-solid representative (`fullRep`) is UNIFORM — kept O(1) (no
//  block, directory entry = -1). Tiles are MATERIALIZED (given a pool block, filled
//  with fullRep) only when a segment's swept bounding box first touches them, so the
//  pool holds only the touched fraction of the grid (it grows on demand via a
//  GPU→GPU copy). prepare() builds only the initially-non-uniform tiles and
//  readResult() reads back only the materialized blocks, emitting fullRep for the
//  untouched bulk — the working-set / init / copyBack reduction.
//
//  The carve kernel (subtract_swept.comp, tiled!=0) reads tile→block from the
//  directory SSBO (binding 5). Output is BYTE-IDENTICAL to the flat backend.
//  Removed-voxel tracking (fitness) is not supported here; countRemoved = 0.
// =============================================================================
class SparseTileBackend : public ICarveBackend {
 public:
  SparseTileBackend() { shader_ = new Shader("shaders/subtract_swept.comp"); }
  ~SparseTileBackend() override {
    delete shader_;
    for (GLuint* b : {&poolBuf_, &countBuf_, &dirBuf_, &toolComp_, &toolPref_, &removedBuf_})
      if (*b) glDeleteBuffers(1, b);
  }
  const char* name() const override { return "sparse"; }

  void prepare(const VoxelObject& stock, const VoxelObject& tool) override {
    params_ = stock.params;
    W_ = stock.params.resolutionXYZ.x;  H_ = stock.params.resolutionXYZ.y;  Z_ = stock.params.resolutionXYZ.z;
    w2_ = tool.params.resolutionXYZ.x;  h2_ = tool.params.resolutionXYZ.y;  z2_ = tool.params.resolutionXYZ.z;
    nX_ = (W_ + TS_ - 1) / TS_;  nY_ = (H_ + TS_ - 1) / TS_;
    nTiles_ = (long)nX_ * nY_;
    const auto& comp = stock.compressedData;
    const auto& pref = stock.prefixSumData;
    const size_t nCols = pref.size();
    auto range = [&](size_t idx, uint32_t& s, uint32_t& e) {
      s = pref[idx];  e = (idx + 1 < nCols) ? pref[idx + 1] : (uint32_t)comp.size();
    };

    // fullRep = the "background" column encoding, taken as column (0,0)'s transitions:
    // for a fresh stock the corner is the untouched representative and every column
    // equals it, so all tiles start UNIFORM. Tiles whose columns differ (a pre-carved
    // region) are simply materialized — always correct, just fewer UNIFORM tiles.
    // (Do NOT assume a specific "full" encoding: this stock stores a solid column as
    // [0, z1-1], not [0, z1].)
    fullRep_.clear();
    if (nCols > 0) {
      uint32_t s, e;  range(0, s, e);
      fullRep_.assign(comp.begin() + s, comp.begin() + e);
    }
    fullCount_ = (int)fullRep_.size();

    // A tile is UNIFORM iff every one of its (in-grid) columns equals fullRep exactly.
    std::vector<char> uniform(nTiles_, fullRep_.empty() ? 0 : 1);
    if (!fullRep_.empty())
      for (int gy = 0; gy < H_; ++gy)
        for (int gx = 0; gx < W_; ++gx) {
          const long tile = tileOf(gx, gy);
          if (!uniform[tile]) continue;
          uint32_t s, e;  range((size_t)gy * W_ + gx, s, e);
          bool same = (e - s) == (uint32_t)fullCount_;
          for (uint32_t k = 0; same && k < (uint32_t)fullCount_; ++k) same = comp[s + k] == fullRep_[k];
          if (!same) uniform[tile] = 0;
        }

    // Assign blocks to the initially-non-uniform tiles and fill them from the stock.
    blockOf_.assign(nTiles_, -1);  nBlocks_ = 0;
    for (long t = 0; t < nTiles_; ++t)
      if (!uniform[t]) blockOf_[t] = (int)nBlocks_++;
    capacity_ = std::max<long>(nBlocks_, 64);
    std::vector<GLuint> pool((size_t)capacity_ * blockUints(), 0u);
    std::vector<GLuint> counts((size_t)capacity_ * countUints(), 0u);
    for (int gy = 0; gy < H_; ++gy)
      for (int gx = 0; gx < W_; ++gx) {
        const int block = blockOf_[tileOf(gx, gy)];
        if (block < 0) continue;
        uint32_t s, e;  range((size_t)gy * W_ + gx, s, e);
        const long lc = localCol(gx, gy);
        counts[(size_t)block * countUints() + lc] = e - s;
        const long base = (long)block * blockUints() + lc * K_;
        for (uint32_t k = s; k < e && (long)(k - s) < K_; ++k) pool[(size_t)base + (k - s)] = comp[k];
      }

    upload(poolBuf_, pool);
    upload(countBuf_, counts);
    upload(dirBuf_, std::vector<GLuint>(blockOf_.begin(), blockOf_.end()));  // int -> GLuint (bit-preserving)
    upload(toolComp_, tool.compressedData);
    upload(toolPref_, tool.prefixSumData);
    upload(removedBuf_, std::vector<GLuint>(1, 0u));
  }

  bool carveSwept(const glm::ivec3& startOffset, const glm::ivec3& displacement, int /*segmentIndex*/) override {
    const int w1 = W_, h1 = H_, z1 = Z_, w2 = w2_, h2 = h2_, z2 = z2_;
    glm::ivec3 endOffset = startOffset + displacement;
    glm::ivec3 tStart(w1 / 2 + startOffset.x, h1 / 2 + startOffset.y, z1 / 2 - startOffset.z);
    glm::ivec3 tEnd(w1 / 2 + endOffset.x, h1 / 2 + endOffset.y, z1 / 2 - endOffset.z);
    glm::ivec3 tDelta = tEnd - tStart;
    glm::ivec3 ad = glm::abs(tDelta);
    int K = glm::max(glm::max(ad.x, ad.y), ad.z);

    long zA = tStart.z < tEnd.z ? tStart.z : tEnd.z, zB = tStart.z > tEnd.z ? tStart.z : tEnd.z;
    if (zB + z2 / 2 <= 0 || zA - z2 / 2 >= z1) return true;  // #8 in-air skip

    long minTx = glm::min(tStart.x, tEnd.x), maxTx = glm::max(tStart.x, tEnd.x);
    long minTy = glm::min(tStart.y, tEnd.y), maxTy = glm::max(tStart.y, tEnd.y);
    long baseX = glm::clamp(minTx - w2 / 2, 0L, (long)w1), endX = glm::clamp(maxTx + w2 / 2, 0L, (long)w1);
    long baseY = glm::clamp(minTy - h2 / 2, 0L, (long)h1), endY = glm::clamp(maxTy + h2 / 2, 0L, (long)h1);
    if (endX <= baseX || endY <= baseY) return true;

    // Materialize every UNIFORM tile the swept bbox touches (so the kernel only ever
    // indexes materialized blocks). Host-side allocation from a growing pool.
    const int txMin = (int)(baseX / TS_), txMax = (int)((endX - 1) / TS_);
    const int tyMin = (int)(baseY / TS_), tyMax = (int)((endY - 1) / TS_);
    for (int ty = tyMin; ty <= tyMax; ++ty)
      for (int tx = txMin; tx <= txMax; ++tx) {
        const long tile = tx + (long)ty * nX_;
        if (blockOf_[tile] < 0) materializeTile(tile);
      }
    if (std::getenv("SPARSE_DBG"))
      std::cerr << "[seg] bbox X[" << baseX << "," << endX << "] Y[" << baseY << "," << endY << "] tiles tx["
                << txMin << ".." << txMax << "] ty[" << tyMin << ".." << tyMax << "] nBlocks=" << nBlocks_ << "\n";

    shader_->use();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, poolBuf_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, countBuf_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, toolComp_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, toolPref_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, removedBuf_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, dirBuf_);
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
    if (std::getenv("SPARSE_DBG"))
      std::cerr << "[sparse] materialized " << nBlocks_ << "/" << nTiles_ << " tiles; pool "
                << (double)capacity_ * blockUints() * sizeof(GLuint) / 1048576.0 << " MB (flat buffer = "
                << (double)W_ * H_ * K_ * sizeof(GLuint) / 1048576.0 << " MB)\n";
    const long bs = blockUints(), cbs = countUints();
    std::vector<GLuint> pool = download(poolBuf_, (size_t)nBlocks_ * bs);      // only materialized blocks
    std::vector<GLuint> counts = download(countBuf_, (size_t)nBlocks_ * cbs);
    out.params = params_;
    out.compressedData.clear();
    out.prefixSumData.assign((size_t)W_ * H_, 0u);
    for (int gy = 0; gy < H_; ++gy)
      for (int gx = 0; gx < W_; ++gx) {
        const size_t idx = (size_t)gy * W_ + gx;
        out.prefixSumData[idx] = (GLuint)out.compressedData.size();
        const int block = blockOf_[tileOf(gx, gy)];
        if (block < 0) {  // UNIFORM tile: emit the full-solid representative
          for (int k = 0; k < fullCount_; ++k) out.compressedData.push_back(fullRep_[k]);
        } else {
          const long lc = localCol(gx, gy), base = (long)block * bs + lc * K_;
          const uint32_t cnt = counts[(size_t)block * cbs + lc];
          for (uint32_t k = 0; k < cnt; ++k) out.compressedData.push_back(pool[(size_t)base + k]);
        }
      }
  }

  // Number of tiles currently materialized (the working-set metric), for benchmarks.
  long materializedTiles() const { return nBlocks_; }

 private:
  long blockUints() const { return (long)TS_ * TS_ * K_; }
  long countUints() const { return (long)TS_ * TS_; }
  long tileOf(int gx, int gy) const { return (gx / TS_) + (long)(gy / TS_) * nX_; }
  long localCol(int gx, int gy) const { return (gx % TS_) + (long)(gy % TS_) * TS_; }
  static int sweptSkip() {
    static const int s = [] { const char* e = std::getenv("AUTOCAM_SWEPT_SKIP"); return (e && e[0] == '0') ? 0 : 1; }();
    return s;
  }

  void materializeTile(long tile) {
    ensureCapacity(nBlocks_ + 1);
    const int block = (int)nBlocks_++;
    blockOf_[tile] = block;
    const long bs = blockUints(), cbs = countUints();
    std::vector<GLuint> blk((size_t)bs, 0u), cnt((size_t)cbs, 0u);
    for (long lc = 0; lc < (long)TS_ * TS_; ++lc) {          // every column of a UNIFORM tile is fullRep
      cnt[(size_t)lc] = (GLuint)fullCount_;
      for (int k = 0; k < fullCount_; ++k) blk[(size_t)lc * K_ + k] = fullRep_[k];
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, poolBuf_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)((size_t)block * bs * sizeof(GLuint)),
                    (GLsizeiptr)(bs * sizeof(GLuint)), blk.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, countBuf_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)((size_t)block * cbs * sizeof(GLuint)),
                    (GLsizeiptr)(cbs * sizeof(GLuint)), cnt.data());
    const GLuint bval = (GLuint)block;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, dirBuf_);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)((size_t)tile * sizeof(GLuint)), (GLsizeiptr)sizeof(GLuint), &bval);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  }

  void ensureCapacity(long need) {
    if (need <= capacity_) return;
    long newCap = capacity_;
    while (newCap < need) newCap *= 2;
    const long bs = blockUints(), cbs = countUints();
    GLuint newPool = 0, newCount = 0;
    glGenBuffers(1, &newPool);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, newPool);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)((size_t)newCap * bs * sizeof(GLuint)), nullptr, GL_DYNAMIC_DRAW);
    glGenBuffers(1, &newCount);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, newCount);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)((size_t)newCap * cbs * sizeof(GLuint)), nullptr, GL_DYNAMIC_DRAW);
    // Copy the materialized blocks GPU->GPU (no CPU round-trip; keeps carved data).
    glBindBuffer(GL_COPY_READ_BUFFER, poolBuf_);  glBindBuffer(GL_COPY_WRITE_BUFFER, newPool);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, (GLsizeiptr)((size_t)nBlocks_ * bs * sizeof(GLuint)));
    glBindBuffer(GL_COPY_READ_BUFFER, countBuf_);  glBindBuffer(GL_COPY_WRITE_BUFFER, newCount);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, (GLsizeiptr)((size_t)nBlocks_ * cbs * sizeof(GLuint)));
    glDeleteBuffers(1, &poolBuf_);  glDeleteBuffers(1, &countBuf_);
    poolBuf_ = newPool;  countBuf_ = newCount;  capacity_ = newCap;
  }

  void upload(GLuint& buf, const std::vector<GLuint>& d) {
    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(d.size() * sizeof(GLuint)), d.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  }
  std::vector<GLuint> download(GLuint buf, size_t n) {
    std::vector<GLuint> d(n);
    if (n) {
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
      glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(n * sizeof(GLuint)), d.data());
      glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    return d;
  }

  Shader* shader_ = nullptr;
  VoxelizationParams params_{};
  int W_ = 0, H_ = 0, Z_ = 0, w2_ = 0, h2_ = 0, z2_ = 0;
  static constexpr int TS_ = 32;  // tile side (columns)
  static constexpr int K_ = 32;   // per-column slots (same as flat MAX_TRANSITIONS)
  int nX_ = 0, nY_ = 0;
  long nTiles_ = 0;
  std::vector<int> blockOf_;      // tile -> block index, or -1 (UNIFORM); CPU mirror of dirBuf_
  std::vector<GLuint> fullRep_;   // full-solid column encoding (the UNIFORM representative)
  int fullCount_ = 0;
  long capacity_ = 0, nBlocks_ = 0;
  GLuint poolBuf_ = 0, countBuf_ = 0, dirBuf_ = 0, toolComp_ = 0, toolPref_ = 0, removedBuf_ = 0;
};
