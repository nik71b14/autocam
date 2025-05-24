#ifndef VOXELIZERZ_HPP
#define VOXELIZERZ_HPP

#include "shader.hpp"
#include "GLUtils.hpp"
#include "voxelizerZUtils.hpp"

void voxelizeZ(
  const MeshBuffers& mesh,
  int triangleCount,
  float zSpan,
  Shader* drawShader,
  Shader* computeShader,
  GLuint fbo,
  GLuint sliceTex, 
  const VoxelizationParams& params
);

#endif // VOXELIZERZ_HPP