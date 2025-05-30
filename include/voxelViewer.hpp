#pragma once

#include <vector>
#include <string>
#include <glad/glad.h> 
#include <GLFW/glfw3.h>
#include "shader.hpp"
#include "voxelizer.hpp"

class VoxelViewer {
public:
  // Constructor from files
  VoxelViewer(
    const std::string& compressedBufferFile,
    const std::string& prefixSumBufferFile,
    VoxelizationParams params);

  // Constructor from memory buffers
  VoxelViewer(
    const std::vector<unsigned int>& compressedData,
    const std::vector<unsigned int>& prefixSumData,
    VoxelizationParams params);

  // Destructor
  ~VoxelViewer();

  // Run render loop
  void run();

private:
  // Members
  VoxelizationParams params;

  std::vector<unsigned int> compressedData;
  std::vector<unsigned int> prefixSumData;

  GLFWwindow* window = nullptr;
  GLuint quadVAO = 0;
  GLuint quadVBO = 0;
  GLuint compressedBuffer = 0;
  GLuint prefixSumBuffer = 0;

  Shader* raymarchingShader = nullptr;

  // Helpers
  void initGL();
  void setupShaderAndBuffers();
  void renderFullScreenQuad();
  bool loadBinaryFile(const std::string& filename, std::vector<unsigned int>& outData);
};

