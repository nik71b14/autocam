#pragma once

// #include <glad/glad.h>
// #include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "shader.hpp"

// Assuming VoxelizationParams is defined somewhere
#include "voxelizer.hpp"  // adjust path as needed

struct GLFWwindow;  // Forward declaration for GLFWwindow

struct VoxelObject {
  VoxelizationParams params;
  std::vector<GLuint> compressedData;
  std::vector<GLuint> prefixSumData;
};

class BoolOps {
 public:
  // BoolOps() = default;
  BoolOps();
  ~BoolOps();

  // Load a voxel object from file and store it internally
  bool load(const std::string& filename);
  bool save(const std::string& filename, int idx = 0);

  // Accessor
  const std::vector<VoxelObject>& getObjects() const { return objects; }
  std::vector<VoxelObject>& getObjects() { return objects; }

  // Optional: clear loaded objects
  void clear() { objects.clear(); }

  // Subtract two voxel objects
  void setupSubtractBuffers(const VoxelObject& obj1, const VoxelObject& obj2);
  bool subtract_old(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset);
  bool subtract(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset);
  bool subtractGPU(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset);

 private:
  std::vector<VoxelObject> objects;
  GLFWwindow* glContext = nullptr;  // OpenGL context for GPU operations

  // GL buffers
  // IN
  GLuint obj1Compressed;
  GLuint obj1Prefix;
  GLuint obj2Compressed;
  GLuint obj2Prefix;
  // OUT
  GLuint outCompressed;
  GLuint outPrefix;
  // Atomic counters
  GLuint atomicCounter;
#ifdef DEBUG_OUTPUT
  GLuint debugCounter;
#endif

  // Shaders
  Shader* shader = nullptr;  // Shader for GPU operations

  // OpenGL utilities
  GLFWwindow* createGLContext();
  void destroyGLContext(GLFWwindow* window);
  GLuint createBuffer(GLsizeiptr size, GLuint binding, GLenum usage);
  GLuint createBuffer(GLsizeiptr size, GLuint binding, GLenum usage, const GLuint* data);
  void loadBuffer(GLuint binding, const std::vector<GLuint>& data);
  std::vector<GLuint> readBuffer(GLuint binding, size_t numElements);
  GLuint createAtomicCounter(GLuint binding);
  void zeroAtomicCounter(GLuint binding);
  void zeroBuffer(GLuint binding);
  GLuint readAtomicCounter(GLuint binding);
};
