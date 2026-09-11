#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <vector>

#include "boolOps.hpp"        // VoxelObject
#include "marchingCubes.hpp"  // for readbackTo(MarchingCubes&)
#include "shader.hpp"

// =============================================================================
//  GpuMarchingCubes — edge-indexed Marching Cubes on the GPU (compute shaders).
//
//  Produces a shared-vertex triangle mesh (smooth, gradient-based normals) from a
//  VoxelObject's transition data, entirely on the GPU. The output lives in two GL
//  buffers (interleaved pos+normal `vbo`, index `ebo`) that can be bound directly
//  by MeshViewer with no CPU round-trip. For STL export / CPU Taubin smoothing,
//  `readbackTo()` copies the mesh into a MarchingCubes' flat buffers.
//
//  Pipeline (reuses prefixSumMultiLevel1B + the voxel occupancy SSBO format):
//    A classify edges -> scan -> B emit vertices ; C classify cells -> scan -> D emit tris.
//
//  Requires a current GL context (like MarchingCubes / MeshViewer).
// =============================================================================
class GpuMarchingCubes {
 public:
  explicit GpuMarchingCubes(const VoxelObject& obj);
  ~GpuMarchingCubes();

  void setStep(int s);  // voxel subsampling (>= 1)

  // Estimated GPU memory (bytes) the run will allocate for the current step — used
  // to abort early with a clear message if it would exceed the budget.
  size_t estimatedBytes() const;

  // Run the full pipeline. Returns false on a clearly-empty result or GL failure.
  bool run();

  // Output (valid after run()).
  GLuint vbo() const { return vboBuf; }
  GLuint ebo() const { return eboBuf; }
  GLsizei indexCount() const { return static_cast<GLsizei>(3u * triTotal); }
  GLuint vertexCount() const { return vertTotal; }
  glm::vec3 bboxMin() const { return bbMin; }
  glm::vec3 bboxMax() const { return bbMax; }

  // True when the whole mesh lives in vbo()/ebo() (single-shot, fits the budget) and
  // can be drawn with no readback. False when the grid was too large and the mesh was
  // streamed in Z-slabs into CPU buffers (use readbackTo / the CPU MeshViewer ctor).
  bool gpuResident() const { return resident; }

  // Read the GPU mesh back into a MarchingCubes (flat verts/normals/indices), for
  // saveStl() and the CPU smooth(N) path. Reuses MC's setVertices/Normals/Triangles.
  void readbackTo(MarchingCubes& mc) const;

 private:
  const VoxelObject* voxelObj = nullptr;
  int step = 1;

  // Virtual padded grid (computed in run()).
  glm::ivec3 pointDims = glm::ivec3(0);  // PX,PY,PZ
  glm::ivec3 cellDims = glm::ivec3(0);   // CX,CY,CZ
  size_t edgeCount = 0;                  // E
  size_t cellCount = 0;                  // C

  glm::vec3 bbMin = glm::vec3(0.0f);
  glm::vec3 bbMax = glm::vec3(0.0f);

  GLuint vertTotal = 0;  // V (single-shot: in vbo; streamed: accumulated total)
  GLuint triTotal = 0;   // T
  bool resident = true;  // see gpuResident()

  // Accumulators for the streamed (large-grid) path: the per-slab meshes are read
  // back and concatenated here, then handed to the CPU MeshViewer ctor / saveStl.
  std::vector<float> accVerts;
  std::vector<float> accNorms;
  std::vector<int> accTris;

  // Occupancy inputs (uploaded from the VoxelObject).
  GLuint transitionsBuf = 0;
  GLuint prefixSumsBuf = 0;
  // Lookup tables.
  GLuint triTableBuf = 0;
  GLuint triCountTableBuf = 0;
  // Working buffers.
  GLuint edgeFlagBuf = 0;
  GLuint edgeVertexIndexBuf = 0;
  GLuint triCountBuf = 0;
  GLuint triPrefixBuf = 0;
  GLuint cubeIndexBuf = 0;
  GLuint blockSumsBuf = 0;
  GLuint blockOffsetsBuf = 0;
  GLuint errorFlagBuf = 0;
  // Outputs.
  GLuint vboBuf = 0;
  GLuint eboBuf = 0;

  Shader* shClassifyEdges = nullptr;
  Shader* shEmitVertices = nullptr;
  Shader* shClassifyCells = nullptr;
  Shader* shEmitTris = nullptr;
  Shader* prefixPass1 = nullptr;
  Shader* prefixPass2 = nullptr;
  Shader* prefixPass3 = nullptr;

  void computeGridDims();
  GLuint createSSBO(size_t bytes);                         // allocate (uninitialised)
  GLuint createSSBOData(const void* data, size_t bytes);   // allocate + upload
  GLuint readUintAt(GLuint buf, size_t index) const;       // single-element readback
  void uploadInputs();                                     // occupancy SSBOs + tables
  void freeBuffers();

  // Run the 4-pass pipeline over a Z-slab (cellZ cells, pointZ points; zBase = the
  // global virtual-cell Z of the slab's first cell minus one). Leaves the slab mesh
  // in vboBuf/eboBuf and sets vertTotal/triTotal for the slab. Single-shot calls it
  // once with the full Z range (zBase = -1).
  bool runPipeline(int zBase, int cellZ, int pointZ);
  void appendSlab(GLuint vBase);  // readback the slab vbo/ebo and append to acc* (indices += vBase)
  void freeSlabBuffers();         // free the per-slab working + output buffers (kept across slabs)
};
