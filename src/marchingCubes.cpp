/*
PROMPT: i have a voxelized object in the form of a compressedData and prefixSumData, i.e. transitions data organized in z columns per each (x,y) coordinate, and
a prefixSumData array which contains the start indices of each z column in the compressedData array. This is in the form of a VoxelObject struct. I want you to
write a marching cubes algorithm which takes this VoxelObject and generates a mesh from it. This is the VoxelObject struct: struct VoxelObject {
  VoxelizationParams params;
  std::vector<GLuint> compressedData;
  std::vector<GLuint> prefixSumData;
};

*/

/*
  Marching Cubes from Compressed Z-Transition Voxel Data
  ------------------------------------------------------

  This implementation generates a triangle mesh from a VoxelObject, which stores
  a voxelized shape using a compressed Z-transition format.

  Input Format:
    - `compressedData`: a flat array storing the Z coordinates of inside/outside transitions
      for each (x, y) column.
    - `prefixSumData`: a companion array where prefixSumData[i] is the start index in
      compressedData for column i = y * resolutionX + x, and prefixSumData[i+1] is the end.

  Role of isInside():
    - `isInsideWithPadding(x, y, z, obj)` checks whether voxel (x, y, z) is inside the solid.
    - It does so by counting how many Z-transitions occur at or below Z.
      - If the count is odd → the voxel is inside.
      - If the count is even → the voxel is outside.
    - This is the key to interpreting the compressed format, and is used at each
      of the cube’s 8 corners to determine surface intersections.

  Algorithm Overview:
    1. Iterate over all voxels in the grid, treating each as the corner of a cube.
    2. Use `isInsideWithPadding()` to determine the binary occupancy of each of the 8 cube corners.
    3. Construct a `cubeIndex` from those 8 bits.
    4. Use `edgeTable[cubeIndex]` to find which edges the surface intersects.
    5. Use `triTable[cubeIndex]` to determine how to connect those intersections into triangles.
    6. Store the generated vertices and triangles in the output buffers.

  Output:
    - `outVertices`: list of vertex positions (glm::vec3).
    - `outTriangles`: list of indexed triangles (glm::ivec3 triplets).

  Notes:
    - The implementation uses midpoint interpolation between corners (sufficient for binary data).
    - Only cubes that intersect the surface (i.e. edgeTable[cubeIndex] ≠ 0) are processed.
*/

#include "marchingCubes.hpp"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "coordinateSystem.hpp"
#include "marching_cubes_tables.hpp"

MarchingCubes::MarchingCubes(const VoxelObject& obj) : voxelObj(&obj) {}

void MarchingCubes::setStep(int s) { step = (s < 1) ? 1 : s; }

const std::vector<float>& MarchingCubes::getVertices() const { return verticesFlat; }

const std::vector<int>& MarchingCubes::getTriangles() const { return trianglesFlat; }

const std::vector<float>& MarchingCubes::getNormals() const { return normalsFlat; }

void MarchingCubes::setVertices(const std::vector<float>& vertices) { verticesFlat = vertices; }

void MarchingCubes::setTriangles(const std::vector<int>& triangles) { trianglesFlat = triangles; }

void MarchingCubes::setNormals(const std::vector<float>& normals) { normalsFlat = normals; }

// bool MarchingCubes::isInsideWithPadding(int x, int y, int z, const VoxelObject& obj) {
//   const auto& p = obj.params;

//   // Allow up to x == resX to detect surface at the last column
//   if (x < 0 || y < 0 || z < 0 || x > p.resolutionXYZ.x || y >= p.resolutionXYZ.y || z >= p.resolutionXYZ.z) return false;

//   // x == resX means it's outside the valid voxel data
//   if (x == p.resolutionXYZ.x) return false;

//   int index = y * p.resolutionXYZ.x + x;
//   GLuint start = obj.prefixSumData[index];
//   GLuint end = obj.prefixSumData[index + 1];

//   bool inside = false;
//   for (GLuint i = start; i < end; ++i) {
//     GLuint zTransition = obj.compressedData[i];
//     if (z < static_cast<int>(zTransition)) break;
//     inside = !inside;
//   }
//   return inside;
// }

bool MarchingCubes::isInsideWithPadding(int x, int y, int z, const VoxelObject& obj) {
  const auto& p = obj.params;

  // Allow one voxel of padding in all directions
  if (x < -1 || y < -1 || z < -1 || x > p.resolutionXYZ.x || y > p.resolutionXYZ.y || z > p.resolutionXYZ.z) {
    return false;
  }

  // For coordinates outside the valid range (padding), consider them outside
  if (x < 0 || y < 0 || z < 0) {
    return false;
  }

  // Special case for maximum boundaries
  if (x >= p.resolutionXYZ.x || y >= p.resolutionXYZ.y || z >= p.resolutionXYZ.z) {
    return false;
  }

  // Normal case - check the voxel data
  int index = y * p.resolutionXYZ.x + x;
  GLuint start = obj.prefixSumData[index];
  // prefixSumData has exactly resX*resY entries (no trailing sentinel), so the last
  // column's end is the total transition count, not prefixSumData[index+1].
  GLuint end = (static_cast<size_t>(index) + 1 < obj.prefixSumData.size()) ? obj.prefixSumData[index + 1]
                                                                           : static_cast<GLuint>(obj.compressedData.size());

  bool inside = false;
  for (GLuint i = start; i < end; ++i) {
    GLuint zTransition = obj.compressedData[i];
    if (z < static_cast<int>(zTransition)) break;
    inside = !inside;
  }
  return inside;
}

/* // With binary search for efficiency (no ok for small numbers of transitions)
bool MarchingCubes::isInsideWithPadding(int x, int y, int z, const VoxelObject& obj) {
  const auto& p = obj.params;

  // Allow coordinates in the range [-1, res] for padding, but discard anything out of bounds
  if (x < -1 || y < -1 || z < -1 || x > p.resolutionXYZ.x || y > p.resolutionXYZ.y || z > p.resolutionXYZ.z) return false;

  if (x < 0 || y < 0 || z < 0 || x >= p.resolutionXYZ.x || y >= p.resolutionXYZ.y || z >= p.resolutionXYZ.z) return false;

  int index = y * p.resolutionXYZ.x + x;
  GLuint start = obj.prefixSumData[index];
  GLuint end = obj.prefixSumData[index + 1];

  auto first = obj.compressedData.begin() + start;
  auto last = obj.compressedData.begin() + end;
  auto upper = std::upper_bound(first, last, static_cast<GLuint>(z));

  int count = std::distance(first, upper);
  return count % 2 == 1;
} */

float MarchingCubes::smoothedScalarField(int x, int y, int z, const VoxelObject& obj) {
  // Can increase the kernel size (e.g., 3×3×3 or even 5×5×5) for more aggressive smoothing, but 6-connected neighbors is fast and good for most cases.
  float sum = 0.0f;
  int count = 0;

  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dz = -1; dz <= 1; ++dz) {
        if (std::abs(dx) + std::abs(dy) + std::abs(dz) > 1) continue;  // 6-neighbors only (optional)
        int nx = x + dx;
        int ny = y + dy;
        int nz = z + dz;
        if (nx < -1 || ny < -1 || nz < -1 || nx > obj.params.resolutionXYZ.x || ny > obj.params.resolutionXYZ.y || nz > obj.params.resolutionXYZ.z) continue;
        sum += isInsideWithPadding(nx, ny, nz, obj) ? 1.0f : 0.0f;
        count++;
      }
    }
  }
  return sum / count;  // Value in [0,1]
}

glm::vec3 MarchingCubes::vertexInterp(const glm::vec3& p1, const glm::vec3& p2) { return (p1 + p2) * 0.5f; }

void MarchingCubes::go() {
  if (!voxelObj) return;
  const auto& obj = *voxelObj;
  const auto& p = obj.params;
  const int resX = p.resolutionXYZ.x;
  const int resY = p.resolutionXYZ.y;
  const int resZ = p.resolutionXYZ.z;

  // Virtual (subsampled) grid: sample one voxel every `step`. Virtual coord n maps
  // to voxel n*step; nX/nY/nZ are sized (ceil) so the far boundary (>= res) is
  // reached, which lets the padding planes close the surface there.
  const int nX = (resX + step - 1) / step;
  const int nY = (resY + step - 1) / step;
  const int nZ = (resZ + step - 1) / step;
  const int nZp2 = nZ + 2;
  const size_t sliceSize = static_cast<size_t>(nY + 2) * nZp2;

  // World position (mm) of a virtual corner, anchored at the CANONICAL grid origin
  // (corner of voxel 0,0,0 = center - extent/2) with `step`-voxel spacing. This
  // matches CoordinateSystem, so the mesh / STL live in the same mm world as the
  // voxel object (the old code offset the mesh by +extent/2).
  const CoordinateSystem cs = CoordinateSystem::fromParams(p);
  const glm::vec3 originMm = cs.originMm();
  const glm::vec3 stepMm = cs.voxelSizeMm * static_cast<float>(step);
  auto pos = [&](int vx, int vy, int vz) -> glm::vec3 { return originMm + glm::vec3(vx, vy, vz) * stepMm; };

  // Occupancy of one virtual X-slice over the padded plane [-1..nY] x [-1..nZ],
  // sampled at voxel (vx*step, vy*step, vz*step). Slices outside the volume are empty.
  auto fillSlice = [&](int vx, std::vector<uint8_t>& buf) {
    const int gx = vx * step;
    if (gx < 0 || gx >= resX) {
      std::fill(buf.begin(), buf.end(), 0);
      return;
    }
    for (int vy = -1; vy <= nY; ++vy) {
      for (int vz = -1; vz <= nZ; ++vz) {
        buf[static_cast<size_t>(vy + 1) * nZp2 + (vz + 1)] = isInsideWithPadding(gx, vy * step, vz * step, obj) ? 1 : 0;
      }
    }
  };
  auto at = [&](const std::vector<uint8_t>& buf, int vy, int vz) -> bool { return buf[static_cast<size_t>(vy + 1) * nZp2 + (vz + 1)] != 0; };

  std::vector<uint8_t> occCurr(sliceSize, 0), occNext(sliceSize, 0);

  verticesFlat.clear();
  trianglesFlat.clear();
  normalsFlat.clear();

  // A cube at virtual (vx,vy,vz) spans corners vx..vx+1, vy..vy+1, vz..vz+1. Iterate
  // vx in [-1, nX-1] so corners cover [-1, nX]: the padding planes at -1 and nX
  // close the surface at every boundary (this also fixes the old missing far-X face).
  fillSlice(-1, occCurr);
  for (int vx = -1; vx < nX; ++vx) {
    glfwPollEvents();  // keep the window responsive during this CPU-heavy pass
    std::cout << "\rMarching Cubes: " << static_cast<int>(100.0f * (vx + 1) / nX) << "%   " << std::flush;

    fillSlice(vx + 1, occNext);

    for (int vy = -1; vy < nY; ++vy) {
      for (int vz = -1; vz < nZ; ++vz) {
        bool cube[8] = {
            at(occCurr, vy, vz),          // 0: (x,   y,   z)
            at(occNext, vy, vz),          // 1: (x+1, y,   z)
            at(occNext, vy + 1, vz),      // 2: (x+1, y+1, z)
            at(occCurr, vy + 1, vz),      // 3: (x,   y+1, z)
            at(occCurr, vy, vz + 1),      // 4: (x,   y,   z+1)
            at(occNext, vy, vz + 1),      // 5: (x+1, y,   z+1)
            at(occNext, vy + 1, vz + 1),  // 6: (x+1, y+1, z+1)
            at(occCurr, vy + 1, vz + 1)   // 7: (x,   y+1, z+1)
        };

        int cubeIndex = 0;
        for (int i = 0; i < 8; ++i)
          if (cube[i]) cubeIndex |= (1 << i);

        if (edgeTable[cubeIndex] == 0) continue;

        glm::vec3 vertList[12];
        if (edgeTable[cubeIndex] & 1) vertList[0] = vertexInterp(pos(vx, vy, vz), pos(vx + 1, vy, vz));
        if (edgeTable[cubeIndex] & 2) vertList[1] = vertexInterp(pos(vx + 1, vy, vz), pos(vx + 1, vy + 1, vz));
        if (edgeTable[cubeIndex] & 4) vertList[2] = vertexInterp(pos(vx + 1, vy + 1, vz), pos(vx, vy + 1, vz));
        if (edgeTable[cubeIndex] & 8) vertList[3] = vertexInterp(pos(vx, vy + 1, vz), pos(vx, vy, vz));
        if (edgeTable[cubeIndex] & 16) vertList[4] = vertexInterp(pos(vx, vy, vz + 1), pos(vx + 1, vy, vz + 1));
        if (edgeTable[cubeIndex] & 32) vertList[5] = vertexInterp(pos(vx + 1, vy, vz + 1), pos(vx + 1, vy + 1, vz + 1));
        if (edgeTable[cubeIndex] & 64) vertList[6] = vertexInterp(pos(vx + 1, vy + 1, vz + 1), pos(vx, vy + 1, vz + 1));
        if (edgeTable[cubeIndex] & 128) vertList[7] = vertexInterp(pos(vx, vy + 1, vz + 1), pos(vx, vy, vz + 1));
        if (edgeTable[cubeIndex] & 256) vertList[8] = vertexInterp(pos(vx, vy, vz), pos(vx, vy, vz + 1));
        if (edgeTable[cubeIndex] & 512) vertList[9] = vertexInterp(pos(vx + 1, vy, vz), pos(vx + 1, vy, vz + 1));
        if (edgeTable[cubeIndex] & 1024) vertList[10] = vertexInterp(pos(vx + 1, vy + 1, vz), pos(vx + 1, vy + 1, vz + 1));
        if (edgeTable[cubeIndex] & 2048) vertList[11] = vertexInterp(pos(vx, vy + 1, vz), pos(vx, vy + 1, vz + 1));

        for (int i = 0; triTable[cubeIndex][i] != -1; i += 3) {
          glm::vec3 v0 = vertList[triTable[cubeIndex][i]];
          glm::vec3 v1 = vertList[triTable[cubeIndex][i + 2]];
          glm::vec3 v2 = vertList[triTable[cubeIndex][i + 1]];

          glm::vec3 faceNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
          int baseIndex = verticesFlat.size() / 3;

          for (int j = 0; j < 3; ++j) {
            glm::vec3 v = (j == 0) ? v0 : (j == 1) ? v1 : v2;
            verticesFlat.push_back(v.x);
            verticesFlat.push_back(v.y);
            verticesFlat.push_back(v.z);

            normalsFlat.push_back(faceNormal.x);
            normalsFlat.push_back(faceNormal.y);
            normalsFlat.push_back(faceNormal.z);

            trianglesFlat.push_back(baseIndex + j);
          }
        }
      }
    }

    std::swap(occCurr, occNext);
  }
  std::cout << "\rMarching Cubes: 100%   " << std::endl;
}

void MarchingCubes::saveStl(const std::string& filename) const {
  std::ofstream out(filename, std::ios::binary);
  if (!out) {
    std::cerr << "Failed to open file for writing: " << filename << "\n";
    return;
  }

  // 80-byte header
  char header[80];
  std::memset(header, 0, 80);
  std::strncpy(header, "Generated by MarchingCubes", 80);
  out.write(header, 80);

  // Number of triangles
  uint32_t numTriangles = trianglesFlat.size() / 3;
  out.write(reinterpret_cast<const char*>(&numTriangles), sizeof(uint32_t));

  for (uint32_t i = 0; i < numTriangles; ++i) {
    uint32_t i0 = trianglesFlat[3 * i + 0];
    uint32_t i1 = trianglesFlat[3 * i + 1];
    uint32_t i2 = trianglesFlat[3 * i + 2];

    glm::vec3 v0(verticesFlat[3 * i0 + 0], verticesFlat[3 * i0 + 1], verticesFlat[3 * i0 + 2]);
    glm::vec3 v1(verticesFlat[3 * i1 + 0], verticesFlat[3 * i1 + 1], verticesFlat[3 * i1 + 2]);
    glm::vec3 v2(verticesFlat[3 * i2 + 0], verticesFlat[3 * i2 + 1], verticesFlat[3 * i2 + 2]);

    // Compute face normal (optional: could use precomputed normal if desired)
    glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

    out.write(reinterpret_cast<const char*>(glm::value_ptr(normal)), sizeof(float) * 3);
    out.write(reinterpret_cast<const char*>(glm::value_ptr(v0)), sizeof(float) * 3);
    out.write(reinterpret_cast<const char*>(glm::value_ptr(v1)), sizeof(float) * 3);
    out.write(reinterpret_cast<const char*>(glm::value_ptr(v2)), sizeof(float) * 3);

    uint16_t attrByteCount = 0;
    out.write(reinterpret_cast<const char*>(&attrByteCount), sizeof(uint16_t));
  }

  out.close();
  std::cout << "Saved mesh as binary STL: " << filename << " (" << numTriangles << " triangles)\n";
}
