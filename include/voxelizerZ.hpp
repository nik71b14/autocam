#pragma once

#include "shader.hpp"
#include "GLUtils.hpp"
#include "voxelizerZUtils.hpp"

void voxelizeZ(
  const std::vector<float>& vertices,
  const std::vector<unsigned int>& indices,
  float zSpan,
  const VoxelizationParams& params
);

