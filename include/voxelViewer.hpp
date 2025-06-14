#pragma once

#include <vector>
#include <string>
//#include <GLFW/glfw3.h>
#include "shader.hpp"
#include "voxelizer.hpp"

struct GLFWwindow; // Forward declaration for GLFW window to avoid including GLFW headers directly here (possible conflict with glad/glad.h)

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

  // Set whether to use orthographic projection
  void setOrthographic(bool useOrtho) { ortho = useOrtho; }

  // Run render loop
  void run();

private:
  // Members
  VoxelizationParams params;
  bool ortho = false; // Use perspective projection by default

  std::vector<unsigned int> compressedData;
  std::vector<unsigned int> prefixSumData;

  GLFWwindow* window = nullptr;
  GLuint quadVAO = 0;
  GLuint quadVBO = 0;
  GLuint compressedBuffer = 0;
  GLuint prefixSumBuffer = 0;

  Shader* raymarchingShader = nullptr;

  // Mouse interaction
  glm::vec2 lastMousePos;
  bool leftMousePressed = false;
  bool rightMousePressed = false;
  bool middleMousePressed = false;
  void onMouseMove(double xpos, double ypos);
  void onMouseButton(int button, int action, int mods);
  void onScroll(double xoffset, double yoffset);

  float pitch = 0.0f;
  float yaw = 90.0f;
  float distance = 2.0f;
  glm::vec3 target = glm::vec3(0.0f);
  glm::vec2 panOffset = glm::vec2(0.0f);


  // Helpers
  void initGL();
  void setupShaderAndBuffers();
  void renderFullScreenQuad();
  bool loadBinaryFile(const std::string& filename, std::vector<unsigned int>& outData);
};

