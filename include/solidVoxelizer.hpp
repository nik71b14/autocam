#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "voxelizerComputeShader.hpp"
#include "meshLoader.hpp"

// Function to perform solid voxelization
// Arguments:
// - stlPath: Path to the STL file.
// - voxelSSBO: OpenGL buffer for voxel data (output).
// - vertexSSBO: OpenGL buffer for vertex data (input).
// - indexSSBO: OpenGL buffer for index data (input).
// - computeShader: Pointer to the compute shader object.
// - bboxMin: Minimum bounding box coordinates of the model.
// - bboxMax: Maximum bounding box coordinates of the model.
void solidVoxelize(const char* stlPath, GLuint& voxelSSBO, GLuint& vertexSSBO, GLuint& indexSSBO, VoxelizerComputeShader* computeShader, const glm::vec3& bboxMin, const glm::vec3& bboxMax);

void solidVoxelizeTransition(
  const char* stlPath,
  GLuint& transitionsSSBO,
  GLuint& vertexSSBO,
  GLuint& indexSSBO,
  VoxelizerComputeShader* computeShader,
  const glm::vec3& bboxMin,
  const glm::vec3& bboxMax);
