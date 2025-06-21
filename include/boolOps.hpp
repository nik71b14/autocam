#pragma once

// #include <glad/glad.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Assuming VoxelizationParams is defined somewhere
#include "voxelizer.hpp"  // adjust path as needed

struct VoxelObject {
  VoxelizationParams params;
  std::vector<GLuint> compressedData;
  std::vector<GLuint> prefixSumData;
};

class BoolOps {
 public:
  BoolOps() = default;
  ~BoolOps() { clear(); }

  // Load a voxel object from file and store it internally
  bool load(const std::string& filename);
  bool save(const std::string& filename, int idx = 0);

  // Accessor
  const std::vector<VoxelObject>& getObjects() const { return objects; }
  std::vector<VoxelObject>& getObjects() { return objects; }

  // Optional: clear loaded objects
  void clear() { objects.clear(); }

  // Subtract two voxel objects
  void test(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset);
  bool subtract_old(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset);
  bool subtract(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset);
  bool subtractGPU(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset);

 private:
  std::vector<VoxelObject> objects;

  // OpenGL utilities
  GLuint createBuffer(GLsizeiptr size, GLuint binding, GLenum usage);
  GLuint createBuffer(GLsizeiptr size, GLuint binding, GLenum usage, const GLuint* data);
  std::vector<GLuint> readBuffer(GLuint binding, size_t numElements);
  GLuint createAtomicCounter(GLuint binding);
  void zeroAtomicCounter(GLuint binding);
  void zeroBuffer(GLuint binding);
  GLuint readAtomicCounter(GLuint binding);
};
