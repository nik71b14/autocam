#pragma once

#include "shader.hpp"
#include "GLUtils.hpp"
#include "voxelizerZUtils.hpp"

std::pair<std::vector<GLuint>, std::vector<GLuint>>  voxelizeZ(
  const std::vector<float>& vertices,
  const std::vector<unsigned int>& indices,
  float zSpan,
  const VoxelizationParams& params
);

