#pragma once

#include <glad/glad.h>
#include <vector>
#include <utility>
#include <glm/glm.hpp>
#include <assimp/scene.h>

// Parameters for voxelization
struct VoxelizationParams {
  int resolutionX = 1024;
  int resolutionY = 1024;
  int resolutionZ = 1024;

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
    Voxelizer(const Mesh& mesh, const VoxelizationParams& params);

    void setMesh(const Mesh& mesh);
    void setParams(const VoxelizationParams& params);

    void run();
    std::pair<std::vector<GLuint>, std::vector<GLuint>> getResults() const;
    float getScale() const { return scale; };

private:
    Mesh mesh;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    VoxelizationParams params;
    float scale = 1.0f; // Scale factor for normalization

    std::vector<GLuint> compressedData;
    std::vector<GLuint> prefixSumData;

    void normalizeMesh();

    std::pair<std::vector<GLuint>, std::vector<GLuint>> voxelizerZ(
      const std::vector<float>& vertices,
      const std::vector<unsigned int>& indices,
      float zSpan,
      const VoxelizationParams& params
    );

    float computeZSpan() const;
    void clearResults();
};
