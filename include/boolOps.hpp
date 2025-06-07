#pragma once

// #include <glad/glad.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>

// Assuming VoxelizationParams is defined somewhere
#include "voxelizer.hpp" // adjust path as needed

class BoolOps {
public:
  BoolOps() = default;
  ~BoolOps() { clear(); }

  struct VoxelObject {
    VoxelizationParams params;
    std::vector<GLuint> compressedData;
    std::vector<GLuint> prefixSumData;
  };

  // Load a voxel object from file and store it internally
  bool load(const std::string& filename);

  // Accessor
  const std::vector<VoxelObject>& getObjects() const { return objects; }

  // Optional: clear loaded objects
  void clear() { objects.clear(); }

  // Subtract two voxel objects
  bool subtract(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset);

private:
  std::vector<VoxelObject> objects;
};
