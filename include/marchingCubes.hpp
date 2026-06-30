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

  // Post-process to remove the "stepped" look: welds coincident vertices, applies
  // `taubinIterations` of Taubin (lambda/mu) smoothing to the geometry, and assigns
  // averaged per-vertex normals (so shading is smooth, not faceted). Call after go();
  // welding + averaged normals happen even at taubinIterations == 0 (exact geometry,
  // smooth shading). Higher iterations = smoother silhouette but rounder sharp edges.
  void smooth(int taubinIterations);

  void saveStl(const std::string& filename) const;

  // Getters
  const std::vector<float>& getVertices() const;
  const std::vector<int>& getTriangles() const;
  const std::vector<float>& getNormals() const;

  // Setters
  void setVertices(const std::vector<float>& vertices);
  void setTriangles(const std::vector<int>& triangles);
  void setNormals(const std::vector<float>& normals);

  // Subsampling: sample one voxel every `s` along each axis (s >= 1). Higher = a
  // coarser, lighter, faster mesh. Default 1 = full resolution. Set before go().
  void setStep(int s);

 private:
  const VoxelObject* voxelObj = nullptr;
  int step = 1;

  std::vector<float> verticesFlat;
  std::vector<int> trianglesFlat;
  std::vector<float> normalsFlat;

  // Helpers for marching cubes
  static bool isInsideWithPadding(int x, int y, int z, const VoxelObject& obj);
  float smoothedScalarField(int x, int y, int z, const VoxelObject& obj);
  glm::vec3 vertexInterp(const glm::vec3& p1, const glm::vec3& p2);
};
