#pragma once

#include <glad/glad.h>
#include "shader.hpp"

void printBufferContents(GLuint buffer, size_t wg_size, size_t numElements, const std::string& message);
void printBufferGraph(GLuint buffer, size_t bufferSize, int numRowsToPrint, char symbol);
// int div_ceil(int x, int y);

inline int div_ceil(int x, int y) {
  return (x + y - 1) / y;
}

void prefixSumMultiLevel1B(
  GLuint countBuffer,
  GLuint prefixSumBuffer,
  GLuint blockSumsBuffer,
  GLuint blockOffsetsBuffer,
  GLuint errorFlagBuffer,
  Shader* prefixPass1,
  Shader* prefixPass2,
  Shader* prefixPass3,
  size_t totalElements,
  int WORKGROUP_SIZE
);
