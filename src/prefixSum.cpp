#include "prefixSum.hpp"

#include <glad/glad.h>
#include <vector>
#include <iostream>
//#include <GL/gl3w.h> // or your GL loader
#include <cassert>
#include "shader.hpp"

// TIP (TODO): ----------------------------------------------------------------
// To avoid dynamic allocations, you can preallocate a pool of buffers for up to 3–4 levels:

// std::vector<GLuint> blockSumsLevels;
// std::vector<GLuint> blockOffsetsLevels;

// Allocate max size of:
// levelN_size = ceil(ceil(ceil(TOTAL_ELEMENTS / 1024) / 1024) / 1024);
// ----------------------------------------------------------------------------

const int WORKGROUP_SIZE = 1024; // max local workgroup size

// Helper: round up integer division
inline int div_ceil(int x, int y) {
    return (x + y - 1) / y;
}


// Multi-level prefix sum dispatcher
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
) {
    // Compute the number of workgroups needed at level 0
    int numBlocks = div_ceil(totalElements, WORKGROUP_SIZE);

    // Store buffers for each level
    std::vector<GLuint> levelBlockSumsBuffers;
    std::vector<GLuint> levelBlockOffsetsBuffers;
    std::vector<int> levelSizes; // number of elements per level

    // Level 0 info
    levelSizes.push_back(totalElements);

    // Allocate buffers for levels > 0
    // We only allocate them as needed and keep references in vectors
    int currentLevelSize = numBlocks;
    int level = 1;
    while (currentLevelSize > 1) {
        levelSizes.push_back(currentLevelSize);
        currentLevelSize = div_ceil(currentLevelSize, WORKGROUP_SIZE);
        level++;
    }
    int numLevels = (int)levelSizes.size();

    // Allocate or re-allocate blockSums and blockOffsets buffers for each level > 0
    levelBlockSumsBuffers.resize(numLevels);
    levelBlockOffsetsBuffers.resize(numLevels);

    for (int i = 1; i < numLevels; ++i) {
        int sz = div_ceil(levelSizes[i-1], WORKGROUP_SIZE);
        sz = levelSizes[i];
        
        // Create buffers for this level if not already
        // For simplicity, create new buffers here (or reuse existing if you want)
        GLuint blockSumsBuf, blockOffsetsBuf;

        glGenBuffers(1, &blockSumsBuf);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockSumsBuf);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sz * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);

        glGenBuffers(1, &blockOffsetsBuf);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockOffsetsBuf);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sz * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);

        levelBlockSumsBuffers[i] = blockSumsBuf;
        levelBlockOffsetsBuffers[i] = blockOffsetsBuf;
    }

    // === PASS 1 at Level 0 ===
    prefixPass1->use();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, countBuffer);         // input counts
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);     // output prefix sums
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, blockSumsBuffer);     // block sums for next level

    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // === PASS 2 and 3 for all upper levels ===
    // For each level > 0, do:
    //  - Pass 2: Scan blockSums from previous level
    //  - Pass 3: Add blockOffsets to prefix sums

    // We'll iterate from level 1 (scanning blockSums from level 0) up to top level
    for (int lvl = 1; lvl < numLevels; ++lvl) {
        int levelSize = levelSizes[lvl];
        int dispatchCount = div_ceil(levelSize, WORKGROUP_SIZE);

        GLuint inputBlockSumsBuf;
        GLuint outputBlockOffsetsBuf;

        if (lvl == 1) {
            // input from blockSumsBuffer from pass1 output
            inputBlockSumsBuf = blockSumsBuffer;
            outputBlockOffsetsBuf = blockOffsetsBuffer;
        } else {
            inputBlockSumsBuf = levelBlockSumsBuffers[lvl];
            outputBlockOffsetsBuf = levelBlockOffsetsBuffers[lvl];
        }

        // Resets error flag buffer for this level
        GLuint errorFlag = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, errorFlagBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &errorFlag);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Pass 2: scan blockSums into blockOffsets for this level
        prefixPass2->use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, inputBlockSumsBuf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outputBlockOffsetsBuf);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, errorFlagBuffer); // optional error flag buffer

        glDispatchCompute(dispatchCount, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Check if we have any errors in the error flag buffer
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, errorFlagBuffer);
        GLuint* ptr = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), GL_MAP_READ_BIT);
        if (ptr) {
            if (*ptr != 0) {
                std::cerr << "Error in prefixPass2 shader: errorFlag = " << *ptr << std::endl;
                // Optionally: break, throw, log, etc.
            }
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        if (dispatchCount > 1) {
            // Recursively scan this level's block sums into next level buffers if not top level yet
            if (lvl + 1 < numLevels) {
                // Prepare next level buffers
                // Copy the current level's block sums buffer to levelBlockSumsBuffers[lvl+1]
                // For simplicity, assume your shaders also write into blockSumsBuffer again
                // or you might need to copy manually or adapt logic here.
            }
        }

        // Pass 3: add blockOffsets back to prefix sums or block sums of previous level
        prefixPass3->use();

        if (lvl == 1) {
            // Add offsets to prefixSumBuffer from pass1 + blockOffsets from pass2 at level 1
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outputBlockOffsetsBuf);
            glDispatchCompute(numBlocks, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        } else {
    // lvl > 1: add offsets from level `lvl` to blockSums from level `lvl - 1`
        pass3BlockLevel->use();  // This is the new shader

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, levelBlockSumsBuffers[lvl - 1]); // destination
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outputBlockOffsetsBuf);          // source offsets
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, errorFlagBuffer);                // optional

        glDispatchCompute(numBlocks, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Optionally: read back errorFlagBuffer here
        }
    }

    // Optional: Check error flag buffer here and handle errors.

    // Cleanup: delete extra buffers created (if desired)
    for (int i = 1; i < numLevels; ++i) {
        glDeleteBuffers(1, &levelBlockSumsBuffers[i]);
        glDeleteBuffers(1, &levelBlockOffsetsBuffers[i]);
    }
}


// Bindings:
// 0	Input data
// 1	Output prefix sum
// 2	Input block sums
// 3	Output block offsets
// 4	Error flag (optional)

// | CPU Code         | Shader Used            | Purpose                                                        |
// |------------------|------------------------|----------------------------------------------------------------|
// | prefixPass1      | pass1.glsl             | Per-block scan + store block sums                              |
// | prefixPass2      | pass2.glsl             | Scan of block sums into block offsets (next level)             |
// | prefixPass3      | pass3.glsl             | Apply block offset to final prefix output (if lvl == 1)        |
// | pass3BlockLevel  | pass3_blocklevel.glsl  | Adjust previous level blockSums with upper-level blockOffsets  |

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
) {
    const int WORKGROUP_SIZE = 1024;
    GLuint numBlocks = div_ceil(totalElements, WORKGROUP_SIZE);


    std::vector<size_t> levelSizes = { totalElements };
    std::vector<GLuint> levelBlockSumsBuffers = { 0, blockSumsBuffer };
    std::vector<GLuint> levelBlockOffsetsBuffers = { 0, blockOffsetsBuffer };

    // Allocate temp buffers for levels ≥ 2
    int currentSize = numBlocks;
    while (currentSize > 1) {
        levelSizes.push_back(currentSize);

        GLuint sumsBuf = 0, offsBuf = 0;
        glGenBuffers(1, &sumsBuf);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, sumsBuf);
        glBufferData(GL_SHADER_STORAGE_BUFFER, currentSize * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);

        glGenBuffers(1, &offsBuf);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, offsBuf);
        glBufferData(GL_SHADER_STORAGE_BUFFER, currentSize * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);

        levelBlockSumsBuffers.push_back(sumsBuf);
        levelBlockOffsetsBuffers.push_back(offsBuf);

        currentSize = div_ceil(currentSize, WORKGROUP_SIZE);
    }

    int numLevels = static_cast<int>(levelSizes.size());

    // ===== LEVEL 0 =====
    prefixPass1->use();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, countBuffer);               // Input data
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);          // Output prefix sum
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, levelBlockSumsBuffers[1]); // Output block sums
    prefixPass1->setUInt("numBlocks", numBlocks);
    prefixPass1->setUInt("numElements", totalElements);

    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ===== LEVELS ≥ 1 =====
    for (int lvl = 1; lvl < numLevels; ++lvl) {
        int levelSize = levelSizes[lvl];
        int dispatchCount = div_ceil(levelSize, WORKGROUP_SIZE);

        GLuint inSums  = levelBlockSumsBuffers[lvl];
        GLuint outOffs = levelBlockOffsetsBuffers[lvl];

        // Clear error flag
        GLuint zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, errorFlagBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // ==== PASS 2: prefix sum of blockSums → blockOffsets ====
        prefixPass2->use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, inSums);    // block sums in
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outOffs);   // block offsets out
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, errorFlagBuffer);
        prefixPass2->setUInt("numBlocks", levelSize);
        prefixPass2->setUInt("numElements", totalElements); 
        
        glDispatchCompute(dispatchCount, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Optional: error checking
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, errorFlagBuffer);
        GLuint* ptr = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), GL_MAP_READ_BIT);
        if (ptr && *ptr != 0)
            std::cerr << "Error in prefixPass2 at level " << lvl << ": errorFlag = " << *ptr << std::endl;
        if (ptr) glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // ==== PASS 3: add blockOffsets to lower level ====
        if (lvl == 1) {
            // Apply to full prefix sum output
            prefixPass3->use();
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outOffs);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, errorFlagBuffer);
            prefixPass3->setUInt("numElements", totalElements);

            glDispatchCompute(numBlocks, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        } else {
            // Apply to previous level block sums
            pass3BlockLevel->use();
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, levelBlockSumsBuffers[lvl - 1]);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outOffs);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, errorFlagBuffer);
            pass3BlockLevel->setUInt("numElements", totalElements);

            int prevBlocks = div_ceil(levelSizes[lvl - 1], WORKGROUP_SIZE);
            glDispatchCompute(prevBlocks, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // Cleanup temporary buffers
    for (int i = 2; i < numLevels; ++i) {
        glDeleteBuffers(1, &levelBlockSumsBuffers[i]);
        glDeleteBuffers(1, &levelBlockOffsetsBuffers[i]);
    }
}
