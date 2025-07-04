#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

// External marching cubes lookup tables
extern const int edgeTable[256];
extern const int triTable[256][16];

// Check if a voxel at (x, y, z) is inside the surface
bool isInside(int x, int y, int z, const VoxelObject& obj);

// Interpolate a vertex along an edge for surface extraction
glm::vec3 vertexInterp(float isoLevel, const glm::vec3& p1, const glm::vec3& p2, bool valp1, bool valp2);

// Generate mesh from a VoxelObject using the Marching Cubes algorithm
void marchingCubes(const VoxelObject& obj, std::vector<glm::vec3>& outVertices, std::vector<glm::ivec3>& outTriangles);