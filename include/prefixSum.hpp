#pragma once

#include <glad/glad.h>
#include "shader.hpp"

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
    GLuint countBuffer,          // input buffer with original counts
    GLuint prefixSumBuffer,      // output prefix sum buffer (size = totalElements + 1)
    GLuint blockSumsBuffer,      // temp: level 0 block sums
    GLuint blockOffsetsBuffer,   // temp: level 0 block offsets
    GLuint errorFlagBuffer,      // optional error flag buffer
    Shader* prefixPass1,         // Level 0 prefix + block sums
    Shader* prefixPass2,         // Higher level prefix (scanning block sums)
    Shader* prefixPass3,         // Add block offsets to level 0 output
    Shader* pass3BlockLevel,     // Add block offsets to block sums at levels > 1
    int totalElements
) ;
