/*
PROMPT: i have a voxelized object in the form of a compressedData and prefixSumData, i.e. transitions data organized in z columns per each (x,y) coordinate, and
a prefixSumData array which contains the start indices of each z column in the compressedData array. This is in the form of a VoxelObject struct. I want you to
write a marching cubes algorithm which takes this VoxelObject and generates a mesh from it. This is the VoxelObject struct: struct VoxelObject {
  VoxelizationParams params;
  std::vector<GLuint> compressedData;
  std::vector<GLuint> prefixSumData;
};

*/

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <vector>
#include "boolOps.hpp"

#include "marching_cubes_tables.hpp"  // edgeTable, triTable

bool isInside(int x, int y, int z, const VoxelObject& obj) {
  const auto& p = obj.params;
  if (x < 0 || y < 0 || z < 0 || x >= p.resolutionXYZ.x || y >= p.resolutionXYZ.y || z >= p.resolutionXYZ.z) return false;

  int index = y * p.resolutionXYZ.x + x;
  GLuint start = obj.prefixSumData[index];
  GLuint end = obj.prefixSumData[index + 1];

  // compressedData stores Z transition coordinates
  bool inside = false;
  for (GLuint i = start; i < end; ++i) {
    GLuint zTransition = obj.compressedData[i];
    if (z < (int)zTransition) break;
    inside = !inside;  // flip state at each transition
  }
  return inside;
}

// Interpolates a vertex on the edge between p1 and p2
glm::vec3 vertexInterp(float isoLevel, const glm::vec3& p1, const glm::vec3& p2, bool valp1, bool valp2) {
  if (std::abs(isoLevel - (valp1 ? 1.0f : 0.0f)) < 0.001f) return p1;
  if (std::abs(isoLevel - (valp2 ? 1.0f : 0.0f)) < 0.001f) return p2;
  return (p1 + p2) * 0.5f;
}

void marchingCubes(const VoxelObject& obj, std::vector<glm::vec3>& outVertices, std::vector<glm::ivec3>& outTriangles) {
  const auto& p = obj.params;
  int resX = p.resolutionXYZ.x;
  int resY = p.resolutionXYZ.y;
  int resZ = p.resolutionXYZ.z;
  float s = p.resolution;

  auto pos = [&](int x, int y, int z) -> glm::vec3 { return p.center + glm::vec3(x * s, y * s, z * s); };

  for (int x = 0; x < resX - 1; ++x) {
    for (int y = 0; y < resY - 1; ++y) {
      //@@@ DEBUG Print progress percent at intervals of 1%
      int total = (resX - 1) * (resY - 1);
      int current = x * (resY - 1) + y;
      // Print progress at intervals of 1%
      static int lastPercent = -1;
      int percent = static_cast<int>(100.0f * current / total);
      if (percent != lastPercent) {
        std::cout << "\rMarching Cubes progress: " << percent << "%   " << std::flush;
        lastPercent = percent;
      }
      //@@@
      for (int z = 0; z < resZ - 1; ++z) {
        bool cube[8] = {isInside(x, y, z, obj),     isInside(x + 1, y, z, obj),     isInside(x + 1, y + 1, z, obj),     isInside(x, y + 1, z, obj),
                        isInside(x, y, z + 1, obj), isInside(x + 1, y, z + 1, obj), isInside(x + 1, y + 1, z + 1, obj), isInside(x, y + 1, z + 1, obj)};

        int cubeIndex = 0;
        for (int i = 0; i < 8; ++i)
          if (cube[i]) cubeIndex |= (1 << i);

        if (edgeTable[cubeIndex] == 0) continue;

        glm::vec3 vertList[12];
        // glm::vec3 p0 = pos(x, y, z); //@@@ DEBUG: uncomment if you want to see the cube vertices

        // Compute interpolated edge vertices
        if (edgeTable[cubeIndex] & 1) vertList[0] = vertexInterp(0.5, pos(x, y, z), pos(x + 1, y, z), cube[0], cube[1]);
        if (edgeTable[cubeIndex] & 2) vertList[1] = vertexInterp(0.5, pos(x + 1, y, z), pos(x + 1, y + 1, z), cube[1], cube[2]);
        if (edgeTable[cubeIndex] & 4) vertList[2] = vertexInterp(0.5, pos(x + 1, y + 1, z), pos(x, y + 1, z), cube[2], cube[3]);
        if (edgeTable[cubeIndex] & 8) vertList[3] = vertexInterp(0.5, pos(x, y + 1, z), pos(x, y, z), cube[3], cube[0]);
        if (edgeTable[cubeIndex] & 16) vertList[4] = vertexInterp(0.5, pos(x, y, z + 1), pos(x + 1, y, z + 1), cube[4], cube[5]);
        if (edgeTable[cubeIndex] & 32) vertList[5] = vertexInterp(0.5, pos(x + 1, y, z + 1), pos(x + 1, y + 1, z + 1), cube[5], cube[6]);
        if (edgeTable[cubeIndex] & 64) vertList[6] = vertexInterp(0.5, pos(x + 1, y + 1, z + 1), pos(x, y + 1, z + 1), cube[6], cube[7]);
        if (edgeTable[cubeIndex] & 128) vertList[7] = vertexInterp(0.5, pos(x, y + 1, z + 1), pos(x, y, z + 1), cube[7], cube[4]);
        if (edgeTable[cubeIndex] & 256) vertList[8] = vertexInterp(0.5, pos(x, y, z), pos(x, y, z + 1), cube[0], cube[4]);
        if (edgeTable[cubeIndex] & 512) vertList[9] = vertexInterp(0.5, pos(x + 1, y, z), pos(x + 1, y, z + 1), cube[1], cube[5]);
        if (edgeTable[cubeIndex] & 1024) vertList[10] = vertexInterp(0.5, pos(x + 1, y + 1, z), pos(x + 1, y + 1, z + 1), cube[2], cube[6]);
        if (edgeTable[cubeIndex] & 2048) vertList[11] = vertexInterp(0.5, pos(x, y + 1, z), pos(x, y + 1, z + 1), cube[3], cube[7]);

        for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
          int i0 = outVertices.size();  // base index for this triangle
          outVertices.push_back(vertList[triTable[cubeIndex][i]]);
          outVertices.push_back(vertList[triTable[cubeIndex][i + 1]]);
          outVertices.push_back(vertList[triTable[cubeIndex][i + 2]]);
          outTriangles.emplace_back(i0, i0 + 1, i0 + 2);
        }
      }
    }
  }
}
