/* #pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// External marching cubes lookup tables
extern const int edgeTable[256];
extern const int triTable[256][16];

// Check if a voxel at (x, y, z) is inside the surface
bool isInside(int x, int y, int z, const VoxelObject& obj);
bool isInsideWithPadding(int x, int y, int z, const VoxelObject& obj);

// Interpolate a vertex along an edge for surface extraction
glm::vec3 vertexInterp(float isoLevel, const glm::vec3& p1, const glm::vec3& p2, bool valp1, bool valp2);

// Generate mesh from a VoxelObject using the Marching Cubes algorithm
void marchingCubes(const VoxelObject& obj, std::vector<float>& outVerticesFlat, std::vector<int>& outTrianglesFlat, std::vector<float>& outNormalsFlat); */

#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "boolOps.hpp"

class MarchingCubes {
 public:
  // Constructors
  explicit MarchingCubes(const VoxelObject& obj);

  // Main API
  void go();
  void saveStl(const std::string& filename) const;

  // Getters
  const std::vector<float>& getVertices() const;
  const std::vector<int>& getTriangles() const;
  const std::vector<float>& getNormals() const;

  // Setters
  void setVertices(const std::vector<float>& vertices);
  void setTriangles(const std::vector<int>& triangles);
  void setNormals(const std::vector<float>& normals);

 private:
  const VoxelObject* voxelObj = nullptr;

  std::vector<float> verticesFlat;
  std::vector<int> trianglesFlat;
  std::vector<float> normalsFlat;

  // Helpers for marching cubes
  static bool isInside(int x, int y, int z, const VoxelObject& obj);
  static bool isInsideWithPadding(int x, int y, int z, const VoxelObject& obj);
  float smoothedScalarField(int x, int y, int z, const VoxelObject& obj);
  //%%%
  static glm::vec3 vertexInterp(float isoLevel, const glm::vec3& p1, const glm::vec3& p2, bool valp1, bool valp2);
  // glm::vec3 vertexInterp(float isoLevel, const glm::vec3& p1, const glm::vec3& p2, float valp1, float valp2);
};
