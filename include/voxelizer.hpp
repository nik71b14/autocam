#pragma once

#include <assimp/scene.h>
#include <glad/glad.h>

#include <glm/glm.hpp>
#include <utility>
#include <vector>

#include "meshLoader.hpp"
#include "meshTypes.hpp"

// Parameters for voxelization (the persisted record for a voxel object).
// CANONICAL scale fields: `resolution` (mm per voxel), `resolutionXYZ` (grid size)
// and `center` (bbox centre, mm); physical extent = resolutionXYZ * resolution.
// `scale`/`zSpan` are DERIVED — kept only for the voxelizer's internal slicing pass
// and the raymarch shader box. Read scale/units through CoordinateSystem
// (coordinateSystem.hpp), not these directly. See DOCS/MANUAL.md
// ("Scale, unità e sistemi di coordinate").
struct VoxelizationParams {
  float resolution = 0.1;                                   // CANONICAL: mm per voxel (object units; unit-agnostic)
  glm::ivec3 resolutionXYZ = glm::ivec3(1024, 1024, 1024);  // CANONICAL: grid size in voxels per axis

  int slicesPerBlock = 32;
  size_t maxMemoryBudgetBytes = 512 * 1024 * 1024;  // 512 MB
  int maxTransitionsPerZColumn = 32;
  glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);   // Default color (white)
  float zSpan = 1.0f;                              // DERIVED: normalized Z extent (resolutionXYZ.z*resolution*scale); shader box only
  glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);  // CANONICAL: bbox centre in world mm
  float scale = 1.0f;                              // DERIVED: 1/max(sizeX,sizeY) mm; slicing/normalization pass only

  bool preview = false;  // Whether to render a preview during voxelization
};

class Voxelizer {
 public:
  Voxelizer();  // Default constructor
  Voxelizer(const VoxelizationParams& params);
  // Voxelizer(const Mesh& mesh, const VoxelizationParams& params);
  Voxelizer(const Mesh& mesh, VoxelizationParams& params);

  void setMesh(const Mesh& mesh);
  void setParams(const VoxelizationParams& params);
  VoxelizationParams getParams() const { return this->params; }

  void run();
  bool save(const std::string& filename);
  std::pair<std::vector<GLuint>, std::vector<GLuint>> getResults() const;
  float getScale() const { return params.scale; };
  glm::ivec3 getResolutionPx() const {
    return params.resolutionXYZ;  // glm::ivec3(params.resolutionXYZ.x, params.resolutionXYZ.y, params.resolutionXYZ.z);
  }
  float getResolution() const { return params.resolution; }

 private:
  Mesh mesh;
  std::vector<float> vertices;
  std::vector<unsigned int> indices;
  VoxelizationParams params;
  // float scale = 1.0f; // Scale factor for normalization

  std::vector<GLuint> compressedData;
  std::vector<GLuint> prefixSumData;

  void normalizeMesh();
  glm::ivec3 calculateResolutionPx(const std::vector<float>& vertices);

  std::pair<std::vector<GLuint>, std::vector<GLuint>> voxelizerZ_OLD(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, float zSpan,
                                                                 const VoxelizationParams& params);
  std::pair<std::vector<GLuint>, std::vector<GLuint>> voxelizerZ(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, float zSpan,
                                                                 const VoxelizationParams& params);

  float computeZSpan() const;
  void clearResults();
};
