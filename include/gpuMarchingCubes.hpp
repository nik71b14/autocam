#pragma once

#include <glad/glad.h>

#include <glm/glm.hpp>

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

  GLuint vertTotal = 0;  // V
  GLuint triTotal = 0;   // T

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
};
