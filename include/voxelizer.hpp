#pragma once

#include <glad/glad.h>
#include <vector>
#include <utility>
#include <glm/glm.hpp>
#include <assimp/scene.h>

// Parameters for voxelization
struct VoxelizationParams {
  float resolution = 0.1; // Resolution referred to the units of the model (agnostic with reference to which units are used)
  glm::ivec3 resolutionXYZ = glm::ivec3(1024, 1024, 1024); // Resolution in pixels for each axis

  int slicesPerBlock = 32;
  size_t maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB
  int maxTransitionsPerZColumn = 32;
  bool preview = false; // Whether to render a preview during voxelization
};

struct Mesh {
  std::vector<float> vertices;
  std::vector<unsigned int> indices;
};

class Voxelizer {
public:
    Voxelizer(); // Default constructor
    Voxelizer(const VoxelizationParams& params);
    //Voxelizer(const Mesh& mesh, const VoxelizationParams& params);
    Voxelizer(const Mesh& mesh, VoxelizationParams& params);

    void setMesh(const Mesh& mesh);
    void setParams(const VoxelizationParams& params);

    void run();
    std::pair<std::vector<GLuint>, std::vector<GLuint>> getResults() const;
    float getScale() const { return scale; };
    glm::ivec3 getResolutionPx() const {
        return params.resolutionXYZ;//glm::ivec3(params.resolutionXYZ.x, params.resolutionXYZ.y, params.resolutionXYZ.z);
    }
    float getResolution() const {
        return params.resolution;
    }

private:
    Mesh mesh;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    VoxelizationParams params;
    float scale = 1.0f; // Scale factor for normalization

    std::vector<GLuint> compressedData;
    std::vector<GLuint> prefixSumData;

    void normalizeMesh();
    glm::ivec3 calculateResolutionPx(const std::vector<float>& vertices);

    std::pair<std::vector<GLuint>, std::vector<GLuint>> voxelizerZ(
      const std::vector<float>& vertices,
      const std::vector<unsigned int>& indices,
      float zSpan,
      const VoxelizationParams& params
    );

    float computeZSpan() const;
    void clearResults();
};
