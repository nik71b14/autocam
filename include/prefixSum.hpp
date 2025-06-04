#pragma once

#include <glad/glad.h>
#include "shader.hpp"

int div_ceil(int x, int y);

void prefixSumMultiLevel1B(
    GLuint countBuffer,
    GLuint prefixSumBuffer,
    GLuint blockSumsBuffer,
    GLuint blockOffsetsBuffer,
    GLuint errorFlagBuffer,
    Shader* prefixPass1,
    Shader* prefixPass2,
    Shader* prefixPass3,
    size_t totalElements
);
