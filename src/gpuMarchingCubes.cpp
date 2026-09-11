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

void GpuMarchingCubes::freeSlabBuffers() {
  GLuint bufs[] = {edgeFlagBuf, edgeVertexIndexBuf, triCountBuf, triPrefixBuf, cubeIndexBuf, blockSumsBuf, blockOffsetsBuf, vboBuf, eboBuf};
  for (GLuint b : bufs)
    if (b) glDeleteBuffers(1, &b);
  edgeFlagBuf = edgeVertexIndexBuf = triCountBuf = triPrefixBuf = cubeIndexBuf = blockSumsBuf = blockOffsetsBuf = vboBuf = eboBuf = 0;
}

// Run the 4-pass pipeline over one Z-slab. pointDims/cellDims carry the full XY; the
// Z extent is `pointZ`/`cellZ` and the slab's global Z is selected by `zBase` (the
// global virtual-cell Z of the slab's first cell, minus one). Single-shot = full Z,
// zBase = -1. Leaves the slab mesh in vboBuf/eboBuf, sets vertTotal/triTotal.
bool GpuMarchingCubes::runPipeline(int zBase, int cellZ, int pointZ) {
  const glm::ivec3 res = voxelObj->params.resolutionXYZ;
  const CoordinateSystem cs = CoordinateSystem::fromParams(voxelObj->params);
  const glm::vec3 originMm = cs.originMm();
  const glm::vec3 stepMm = cs.voxelSizeMm * (float)step;

  const glm::ivec3 pDims(pointDims.x, pointDims.y, pointZ);
  const glm::ivec3 cDims(cellDims.x, cellDims.y, cellZ);
  const size_t E = (size_t)pDims.x * pDims.y * pDims.z * 3;
  const size_t C = (size_t)cDims.x * cDims.y * cDims.z;

  freeSlabBuffers();
  edgeFlagBuf = createSSBO(E * sizeof(GLuint));
  edgeVertexIndexBuf = createSSBO(E * sizeof(GLuint));
  triCountBuf = createSSBO(C * sizeof(GLuint));
  triPrefixBuf = createSSBO(C * sizeof(GLuint));
  cubeIndexBuf = createSSBO(C * sizeof(GLuint));
  const size_t numBlocks = (std::max(E, C) + 1023) / 1024;
  blockSumsBuf = createSSBO(numBlocks * sizeof(GLuint));
  blockOffsetsBuf = createSSBO(numBlocks * sizeof(GLuint));
  if (!errorFlagBuf) errorFlagBuf = createSSBO(sizeof(GLuint));

  // --- Pass A: classify edges ---
  shClassifyEdges->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transitionsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, edgeFlagBuf);
  shClassifyEdges->setIVec3("resolution", res);
  shClassifyEdges->setIVec3("pointDims", pDims);
  shClassifyEdges->setInt("step", step);
  shClassifyEdges->setInt("zBase", zBase);
  glDispatchCompute(groups(pDims.x), groups(pDims.y), groups(pDims.z));
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // --- Scan A ---
  prefixSumMultiLevel1B(edgeFlagBuf, edgeVertexIndexBuf, blockSumsBuf, blockOffsetsBuf, errorFlagBuf, prefixPass1, prefixPass2, prefixPass3, E, 1024);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glFinish();
  vertTotal = readUintAt(edgeVertexIndexBuf, E - 1) + readUintAt(edgeFlagBuf, E - 1);

  // --- Pass B: emit vertices ---
  vboBuf = createSSBO(std::max<size_t>(1, (size_t)vertTotal) * 6 * sizeof(float));
  shEmitVertices->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transitionsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, edgeFlagBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, edgeVertexIndexBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, vboBuf);
  shEmitVertices->setIVec3("resolution", res);
  shEmitVertices->setIVec3("pointDims", pDims);
  shEmitVertices->setInt("step", step);
  shEmitVertices->setInt("zBase", zBase);
  shEmitVertices->setVec3("originMm", originMm);
  shEmitVertices->setVec3("stepMm", stepMm);
  glDispatchCompute(groups(pDims.x), groups(pDims.y), groups(pDims.z));
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // --- Pass C: classify cells ---
  shClassifyCells->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transitionsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumsBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, triCountTableBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, triCountBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, cubeIndexBuf);
  shClassifyCells->setIVec3("resolution", res);
  shClassifyCells->setIVec3("cellDims", cDims);
  shClassifyCells->setInt("step", step);
  shClassifyCells->setInt("zBase", zBase);
  glDispatchCompute(groups(cDims.x), groups(cDims.y), groups(cDims.z));
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // --- Scan C ---
  prefixSumMultiLevel1B(triCountBuf, triPrefixBuf, blockSumsBuf, blockOffsetsBuf, errorFlagBuf, prefixPass1, prefixPass2, prefixPass3, C, 1024);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glFinish();
  triTotal = readUintAt(triPrefixBuf, C - 1) + readUintAt(triCountBuf, C - 1);
  if (triTotal == 0 || vertTotal == 0) return false;  // empty slab

  // --- Pass D: emit triangle indices ---
  eboBuf = createSSBO((size_t)triTotal * 3 * sizeof(GLuint));
  shEmitTris->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triTableBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triPrefixBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cubeIndexBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, edgeVertexIndexBuf);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, eboBuf);
  shEmitTris->setIVec3("cellDims", cDims);
  shEmitTris->setIVec3("pointDims", pDims);
  glDispatchCompute(groups(cDims.x), groups(cDims.y), groups(cDims.z));
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
  glFinish();
  return true;
}

void GpuMarchingCubes::appendSlab(GLuint vBase) {
  std::vector<float> inter((size_t)vertTotal * 6);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, vboBuf);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(inter.size() * sizeof(float)), inter.data());
  accVerts.reserve(accVerts.size() + (size_t)vertTotal * 3);
  accNorms.reserve(accNorms.size() + (size_t)vertTotal * 3);
  for (size_t i = 0; i < vertTotal; ++i) {
    accVerts.push_back(inter[i * 6 + 0]);
    accVerts.push_back(inter[i * 6 + 1]);
    accVerts.push_back(inter[i * 6 + 2]);
    accNorms.push_back(inter[i * 6 + 3]);
    accNorms.push_back(inter[i * 6 + 4]);
    accNorms.push_back(inter[i * 6 + 5]);
  }
  std::vector<GLuint> idx((size_t)triTotal * 3);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, eboBuf);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, (GLsizeiptr)(idx.size() * sizeof(GLuint)), idx.data());
  accTris.reserve(accTris.size() + idx.size());
  for (GLuint v : idx) accTris.push_back(static_cast<int>(v + vBase));
}

bool GpuMarchingCubes::run() {
  computeGridDims();
  uploadInputs();

  const size_t budget = static_cast<size_t>(2) * 1024 * 1024 * 1024;  // 2 GiB working-set target

  if (estimatedBytes() <= budget) {
    // Single-shot: whole grid at once, mesh stays GPU-resident (zero-readback view).
    resident = true;
    if (!runPipeline(/*zBase=*/-1, cellDims.z, pointDims.z)) {
      std::cout << "GPU Marching Cubes: empty mesh (no surface)." << std::endl;
      return false;
    }
    std::cout << "GPU Marching Cubes: " << vertTotal << " verts, " << triTotal << " tris (step " << step << ")" << std::endl;
    return true;
  }

  // Streamed: split the global cube-Z range [0, CZ) into Z-slabs sized to fit the
  // budget; read each slab back and concatenate. Boundary-plane vertices duplicate
  // between adjacent slabs but with identical pos+normal, so the surface is seamless.
  resident = false;
  const int CZ = cellDims.z;
  const size_t perCellZ = static_cast<size_t>(2 * 3 * pointDims.x * pointDims.y + 3 * cellDims.x * cellDims.y) * sizeof(GLuint);
  int slabCellZ = static_cast<int>(std::max<size_t>(1, (budget * 7 / 10) / std::max<size_t>(1, perCellZ)));
  slabCellZ = std::min(slabCellZ, CZ);

  accVerts.clear();
  accNorms.clear();
  accTris.clear();
  GLuint vAccum = 0;
  int slabs = 0;
  for (int czStart = 0; czStart < CZ; czStart += slabCellZ) {
    const int cz = std::min(slabCellZ, CZ - czStart);
    if (runPipeline(czStart - 1, cz, cz + 1)) {
      appendSlab(vAccum);
      vAccum += vertTotal;
    }
    ++slabs;
  }
  freeSlabBuffers();  // last slab's GPU buffers; the mesh now lives in the acc* vectors
  vertTotal = vAccum;
  triTotal = static_cast<GLuint>(accTris.size() / 3);
  if (triTotal == 0) {
    std::cout << "GPU Marching Cubes: empty mesh (no surface)." << std::endl;
    return false;
  }
  std::cout << "GPU Marching Cubes (streamed, " << slabs << " Z-slabs): " << vertTotal << " verts, " << triTotal << " tris (step " << step << ")" << std::endl;
  return true;
}

void GpuMarchingCubes::readbackTo(MarchingCubes& mc) const {
  if (!resident) {
    // Streamed: the mesh is already on the CPU in the accumulators.
    mc.setVertices(accVerts);
    mc.setNormals(accNorms);
    mc.setTriangles(accTris);
    return;
  }
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
