#ifndef VOXELIZERZ_HPP
#define VOXELIZERZ_HPP

#include "shader.hpp"
#include "GLUtils.hpp"

void voxelizeZ(
  const MeshBuffers& mesh,
  int triangleCount,
  float zSpan,
  Shader* drawShader,
  Shader* computeShader,
  GLuint fbo,
  GLuint sliceTex,
  int resolution,
  int resolutionZ,
  int slicesPerBlock//,
  //bool preview
);

#endif // VOXELIZERZ_HPP