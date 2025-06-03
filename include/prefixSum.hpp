#pragma once

#include <glad/glad.h>
#include "shader.hpp"

int div_ceil(int x, int y);

void prefixSumMultiLevel1M(
    GLuint countBuffer,          // input buffer with original counts
    GLuint prefixSumBuffer,      // output prefix sum buffer (size = totalPixels + 1)
    GLuint blockSumsBuffer,      // temporary buffer for block sums
    GLuint blockOffsetsBuffer,   // temporary buffer for block offsets
    GLuint errorFlagBuffer,      // optional error flag buffer
    Shader* prefixPass1,
    Shader* prefixPass2,
    Shader* prefixPass3,
    Shader* pass3BlockLevel,     // new shader for pass 3 at block level
    int totalElements            // total number of elements to scan
);

void prefixSumMultiLevel1B(
    GLuint countBuffer,
    GLuint prefixSumBuffer,
    GLuint blockSumsBuffer,
    GLuint blockOffsetsBuffer,
    GLuint errorFlagBuffer,
    Shader* prefixPass1,
    Shader* prefixPass2,
    Shader* prefixPass3,
    Shader* pass3BlockLevel,
    size_t totalElements
);
