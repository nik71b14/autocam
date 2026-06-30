#include "gpuMarchingCubes.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

#include "coordinateSystem.hpp"
#include "marching_cubes_tables.hpp"  // triTable[256][16]
#include "prefixSum.hpp"

namespace {
constexpr int LOCAL = 4;                                   // local_size_{x,y,z} of the MC passes
inline GLuint groups(int dim) { return (GLuint)((dim + LOCAL - 1) / LOCAL); }
}  // namespace

GpuMarchingCubes::GpuMarchingCubes(const VoxelObject& obj) : voxelObj(&obj) {
  shClassifyEdges = new Shader("shaders/mc_classify_edges.comp");
  shEmitVertices = new Shader("shaders/mc_emit_vertices.comp");
  shClassifyCells = new Shader("shaders/mc_classify_cells.comp");
  shEmitTris = new Shader("shaders/mc_emit_tris.comp");
  prefixPass1 = new Shader("shaders/prefix_pass1.comp");
  prefixPass2 = new Shader("shaders/prefix_pass2.comp");
  prefixPass3 = new Shader("shaders/prefix_pass3.comp");
}

GpuMarchingCubes::~GpuMarchingCubes() {
  freeBuffers();
  delete shClassifyEdges;
  delete shEmitVertices;
  delete shClassifyCells;
  delete shEmitTris;
  delete prefixPass1;
  delete prefixPass2;
  delete prefixPass3;
}

void GpuMarchingCubes::setStep(int s) { step = (s < 1) ? 1 : s; }

void GpuMarchingCubes::computeGridDims() {
  const auto& r = voxelObj->params.resolutionXYZ;
  const int nX = (r.x + step - 1) / step;
  const int nY = (r.y + step - 1) / step;
  const int nZ = (r.z + step - 1) / step;
  pointDims = glm::ivec3(nX + 2, nY + 2, nZ + 2);  // padded lattice points
  cellDims = glm::ivec3(nX + 1, nY + 1, nZ + 1);   // padded cells
  edgeCount = (size_t)pointDims.x * pointDims.y * pointDims.z * 3;
  cellCount = (size_t)cellDims.x * cellDims.y * cellDims.z;

  const CoordinateSystem cs = CoordinateSystem::fromParams(voxelObj->params);
  bbMin = cs.originMm();
  bbMax = cs.originMm() + cs.extentMm();
}

size_t GpuMarchingCubes::estimatedBytes() const {
  // The volume-proportional working set: edgeFlag+edgeVertexIndex (E uints each)
  // and triCount+triPrefix+cubeIndex (C uints each). vbo/ebo (surface-proportional)
  // are unknown pre-run but small next to this.
  return (2 * edgeCount + 3 * cellCount) * sizeof(GLuint);
}

GLuint GpuMarchingCubes::createSSBO(size_t bytes) {
  GLuint b = 0;
  glGenBuffers(1, &b);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, b);
  glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)bytes, nullptr, GL_DYNAMIC_COPY);
  return b;
}

GLuint GpuMarchingCubes::createSSBOData(const void* data, size_t bytes) {
  GLuint b = 0;
  glGenBuffers(1, &b);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, b);
  glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)bytes, data, GL_DYNAMIC_COPY);
  return b;
}

GLuint GpuMarchingCubes::readUintAt(GLuint buf, size_t index) const {
  GLuint v = 0;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)(index * sizeof(GLuint)), sizeof(GLuint), &v);
  return v;
}

void GpuMarchingCubes::uploadInputs() {
  // Occupancy SSBOs (same format/bindings as the raymarch viewer).
  transitionsBuf = createSSBOData(voxelObj->compressedData.data(), voxelObj->compressedData.size() * sizeof(GLuint));
  prefixSumsBuf = createSSBOData(voxelObj->prefixSumData.data(), voxelObj->prefixSumData.size() * sizeof(GLuint));

  // triTable flattened to 256*16 ints; triCountTable derived (triangles per cube index).
  std::vector<int> triFlat(256 * 16);
  std::vector<GLuint> triCnt(256);
  for (int i = 0; i < 256; ++i) {
    int n = 0;
    for (int j = 0; j < 16; ++j) {
      triFlat[i * 16 + j] = triTable[i][j];
      if (triTable[i][j] != -1) ++n;
    }
    triCnt[i] = (GLuint)(n / 3);
  }
  triTableBuf = createSSBOData(triFlat.data(), triFlat.size() * sizeof(int));
  triCountTableBuf = createSSBOData(triCnt.data(), triCnt.size() * sizeof(GLuint));
}

void GpuMarchingCubes::freeBuffers() {
  GLuint bufs[] = {transitionsBuf, prefixSumsBuf, triTableBuf,    triCountTableBuf, edgeFlagBuf, edgeVertexIndexBuf, triCountBuf,
                   triPrefixBuf,   cubeIndexBuf,  blockSumsBuf,   blockOffsetsBuf,  errorFlagBuf, vboBuf,            eboBuf};
  for (GLuint b : bufs)
    if (b) glDeleteBuffers(1, &b);
  transitionsBuf = prefixSumsBuf = triTableBuf = triCountTableBuf = edgeFlagBuf = edgeVertexIndexBuf = triCountBuf = triPrefixBuf = cubeIndexBuf =
      blockSumsBuf = blockOffsetsBuf = errorFlagBuf = vboBuf = eboBuf = 0;
}

bool GpuMarchingCubes::run() {
  computeGridDims();

  // Budget guard: the working buffers are proportional to the virtual VOLUME, so at
  // low --mesh-step on a big object they can be enormous. Abort early with a clear
  // hint instead of crashing / OOMing the driver. (The CPU path streams and handles
  // full resolution; the GPU path is meant for interactive, subsampled meshes.)
  const size_t est = estimatedBytes();
  const size_t budget = static_cast<size_t>(2) * 1024 * 1024 * 1024;  // 2 GiB
  if (est > budget) {
    std::cerr << "GPU Marching Cubes needs ~" << (est >> 20) << " MB of GPU buffers for grid " << pointDims.x << "x" << pointDims.y << "x" << pointDims.z
              << " — too large. Raise --mesh-step (or use --cpu for full resolution)." << std::endl;
    return false;
  }

  uploadInputs();

  const glm::ivec3 res = voxelObj->params.resolutionXYZ;
  const CoordinateSystem cs = CoordinateSystem::fromParams(voxelObj->params);
  const glm::vec3 originMm = cs.originMm();
  const glm::vec3 stepMm = cs.voxelSizeMm * (float)step;

  // Working buffers.
  edgeFlagBuf = createSSBO(edgeCount * sizeof(GLuint));
  edgeVertexIndexBuf = createSSBO(edgeCount * sizeof(GLuint));
  triCountBuf = createSSBO(cellCount * sizeof(GLuint));
  triPrefixBuf = createSSBO(cellCount * sizeof(GLuint));
  cubeIndexBuf = createSSBO(cellCount * sizeof(GLuint));
  const size_t numBlocks = (std::max(edgeCount, cellCount) + 1023) / 1024;
  blockSumsBuf = createSSBO(numBlocks * sizeof(GLuint));
  blockOffsetsBuf = createSSBO(numBlocks * sizeof(GLuint));
  errorFlagBuf = createSSBO(sizeof(GLuint));

  // --- Pass A: classify edges -----------------------------------------------------
  shClassifyEdges->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transitionsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, edgeFlagBuf);
  shClassifyEdges->setIVec3("resolution", res);
  shClassifyEdges->setIVec3("pointDims", pointDims);
  shClassifyEdges->setInt("step", step);
  glDispatchCompute(groups(pointDims.x), groups(pointDims.y), groups(pointDims.z));
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // --- Scan A: edge flags -> per-edge vertex index --------------------------------
  prefixSumMultiLevel1B(edgeFlagBuf, edgeVertexIndexBuf, blockSumsBuf, blockOffsetsBuf, errorFlagBuf, prefixPass1, prefixPass2, prefixPass3, edgeCount, 1024);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glFinish();
  vertTotal = readUintAt(edgeVertexIndexBuf, edgeCount - 1) + readUintAt(edgeFlagBuf, edgeCount - 1);

  // --- Pass B: emit vertices ------------------------------------------------------
  vboBuf = createSSBO(std::max<size_t>(1, (size_t)vertTotal) * 6 * sizeof(float));
  shEmitVertices->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transitionsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, edgeFlagBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, edgeVertexIndexBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, vboBuf);
  shEmitVertices->setIVec3("resolution", res);
  shEmitVertices->setIVec3("pointDims", pointDims);
  shEmitVertices->setInt("step", step);
  shEmitVertices->setVec3("originMm", originMm);
  shEmitVertices->setVec3("stepMm", stepMm);
  glDispatchCompute(groups(pointDims.x), groups(pointDims.y), groups(pointDims.z));
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // --- Pass C: classify cells -----------------------------------------------------
  shClassifyCells->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transitionsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, triCountTableBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, triCountBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cubeIndexBuf);
  shClassifyCells->setIVec3("resolution", res);
  shClassifyCells->setIVec3("cellDims", cellDims);
  shClassifyCells->setInt("step", step);
  glDispatchCompute(groups(cellDims.x), groups(cellDims.y), groups(cellDims.z));
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // --- Scan C: per-cell triangle counts -> write offsets --------------------------
  prefixSumMultiLevel1B(triCountBuf, triPrefixBuf, blockSumsBuf, blockOffsetsBuf, errorFlagBuf, prefixPass1, prefixPass2, prefixPass3, cellCount, 1024);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glFinish();
  triTotal = readUintAt(triPrefixBuf, cellCount - 1) + readUintAt(triCountBuf, cellCount - 1);

  if (triTotal == 0 || vertTotal == 0) {
    std::cout << "GPU Marching Cubes: empty mesh (no surface)." << std::endl;
    return false;
  }

  // --- Pass D: emit triangle indices ----------------------------------------------
  eboBuf = createSSBO((size_t)triTotal * 3 * sizeof(GLuint));
  shEmitTris->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triTableBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triPrefixBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cubeIndexBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, edgeVertexIndexBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, eboBuf);
  shEmitTris->setIVec3("cellDims", cellDims);
  shEmitTris->setIVec3("pointDims", pointDims);
  glDispatchCompute(groups(cellDims.x), groups(cellDims.y), groups(cellDims.z));
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
  glFinish();

  std::cout << "GPU Marching Cubes: " << vertTotal << " verts, " << triTotal << " tris (step " << step << ")" << std::endl;
  return true;
}

void GpuMarchingCubes::readbackTo(MarchingCubes& mc) const {
  std::vector<float> interleaved((size_t)vertTotal * 6);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, vboBuf);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(interleaved.size() * sizeof(float)), interleaved.data());

  std::vector<float> verts((size_t)vertTotal * 3);
  std::vector<float> norms((size_t)vertTotal * 3);
  for (size_t i = 0; i < vertTotal; ++i) {
    verts[i * 3 + 0] = interleaved[i * 6 + 0];
    verts[i * 3 + 1] = interleaved[i * 6 + 1];
    verts[i * 3 + 2] = interleaved[i * 6 + 2];
    norms[i * 3 + 0] = interleaved[i * 6 + 3];
    norms[i * 3 + 1] = interleaved[i * 6 + 4];
    norms[i * 3 + 2] = interleaved[i * 6 + 5];
  }

  std::vector<GLuint> idx((size_t)triTotal * 3);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, eboBuf);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(idx.size() * sizeof(GLuint)), idx.data());
  std::vector<int> tris(idx.begin(), idx.end());

  mc.setVertices(verts);
  mc.setNormals(norms);
  mc.setTriangles(tris);
}
