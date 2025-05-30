#pragma once

#include <glad/glad.h>
#include <vector>
#include <utility>
#include <glm/glm.hpp>

// Parameters for voxelization
struct VoxelizationParams {
  int resolution = 512;
  int resolutionZ = 512; // Default Z resolution
  int slicesPerBlock = 32;
  size_t maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB
  int maxTransitionsPerZColumn = 32;
  bool preview = false; // Whether to render a preview during voxelization
};

class Voxelizer {
public:
    Voxelizer(); // Default constructor
    Voxelizer(const VoxelizationParams& params);
    Voxelizer(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, const VoxelizationParams& params);

    void setVertices(const std::vector<float>& vertices);
    void setIndices(const std::vector<unsigned int>& indices);
    void setParams(const VoxelizationParams& params);

    void run();
    std::pair<std::vector<GLuint>, std::vector<GLuint>> getResults() const;

private:
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    VoxelizationParams params;

    std::vector<GLuint> compressedData;
    std::vector<GLuint> prefixSumData;

    std::pair<std::vector<GLuint>, std::vector<GLuint>> voxelizerZ(
      const std::vector<float>& vertices,
      const std::vector<unsigned int>& indices,
      float zSpan,
      const VoxelizationParams& params
    );

    float computeZSpan() const;
    void clearResults();
};
