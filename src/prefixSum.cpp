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
    size_t totalElements
) {
    const int WORKGROUP_SIZE = 1024;
    GLuint numBlocks = div_ceil(totalElements, WORKGROUP_SIZE);
    std::cout << "numBlocks = " << numBlocks << std::endl;


    std::vector<size_t> levelSizes = { totalElements };
    std::vector<GLuint> levelBlockSumsBuffers = { 0, blockSumsBuffer };
    std::vector<GLuint> levelBlockOffsetsBuffers = { 0, blockOffsetsBuffer };

    // Calculate how many levels we need and each level's size
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

    // Calculate number of levels
    int numLevels = static_cast<int>(levelSizes.size());

    // LEVEL 0: up to 1024 elements is the only level to be executed
    prefixPass1->use();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, countBuffer);                 // Input data
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);             // Output prefix sum
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, levelBlockSumsBuffers[1]);    // Output block sums
    prefixPass1->setUInt("numBlocks", numBlocks);
    prefixPass1->setUInt("numElements", totalElements);

    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    //@@@ PASS1 CHECK
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
    GLuint* prefixData = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, (totalElements + 1) * sizeof(GLuint), GL_MAP_READ_BIT);

    if (prefixData) {

        std::cout << "prefixData from level 0 (size " << (totalElements + 1) << "): ";
        if (totalElements + 1 >= 6) {
            std::cout << prefixData[0] << ", " << prefixData[1] << ", " << prefixData[2]
                      << ", ..., "
                      << prefixData[totalElements - 2] << ", " << prefixData[totalElements - 1] << ", " << prefixData[totalElements];
        } else {
            for (size_t i = 0; i < totalElements + 1; ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << prefixData[i];
            }
        }
        std::cout << std::endl;

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    //@@@ END PASS1 CHECK
    

    // LEVELS ≥ 1: only if we have more than 1024 elements
    for (int lvl = 1; lvl < numLevels; ++lvl) {
        int levelSize = levelSizes[lvl];
        int dispatchCount = div_ceil(levelSize, WORKGROUP_SIZE);

        GLuint inSums  = levelBlockSumsBuffers[lvl]; // Block sums from the previous level, i.e. last element of each block prefix sum
        GLuint outOffs = levelBlockOffsetsBuffers[lvl]; // 

        //@@@ (show the block sums for this level)
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, inSums);
        GLuint* inSumsData = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint) * levelSize, GL_MAP_READ_BIT);
        if (inSumsData) {
            std::cout << "block sums from level " << lvl - 1 << ", size " << levelSize << "):" << std::endl;
            size_t numBlocksToShow = div_ceil(levelSize, WORKGROUP_SIZE);
            size_t blocksToPrint = 3;
            // Print first 3 blocks
            for (size_t block = 0; block < std::min(blocksToPrint, numBlocksToShow); ++block) {
            size_t start = block * WORKGROUP_SIZE;
            size_t end = std::min(start + WORKGROUP_SIZE, (size_t)levelSize);
            std::cout << "Block " << block << " (elements " << start << " to " << end - 1 << "): ";
            size_t blockSize = end - start;
            if (blockSize <= 6) {
                for (size_t i = start; i < end; ++i) {
                if (i > start) std::cout << ", ";
                std::cout << inSumsData[i];
                }
            } else {
                for (size_t i = 0; i < 3; ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << inSumsData[start + i];
                }
                std::cout << ", ..., ";
                for (size_t i = blockSize - 3; i < blockSize; ++i) {
                if (i > blockSize - 3) std::cout << ", ";
                std::cout << inSumsData[start + i];
                }
            }
            std::cout << std::endl;
            }
            if (numBlocksToShow > 2 * blocksToPrint) {
            std::cout << "......" << std::endl;
            }
            // Print last 3 blocks
            for (size_t block = (numBlocksToShow > blocksToPrint ? numBlocksToShow - blocksToPrint : blocksToPrint); block < numBlocksToShow; ++block) {
            size_t idx = block;
            if (numBlocksToShow > 2 * blocksToPrint)
                idx = numBlocksToShow - blocksToPrint + (block - (numBlocksToShow - blocksToPrint));
            if (idx < blocksToPrint) continue; // skip if already printed in first loop
            size_t start = idx * WORKGROUP_SIZE;
            size_t end = std::min(start + WORKGROUP_SIZE, (size_t)levelSize);
            std::cout << "Block " << idx << " (elements " << start << " to " << end - 1 << "): ";
            size_t blockSize = end - start;
            if (blockSize <= 6) {
                for (size_t i = start; i < end; ++i) {
                if (i > start) std::cout << ", ";
                std::cout << inSumsData[i];
                }
            } else {
                for (size_t i = 0; i < 3; ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << inSumsData[start + i];
                }
                std::cout << ", ..., ";
                for (size_t i = blockSize - 3; i < blockSize; ++i) {
                if (i > blockSize - 3) std::cout << ", ";
                std::cout << inSumsData[start + i];
                }
            }
            std::cout << std::endl;
            }
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        //@@@

        // Clear error flag
        GLuint zero = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, errorFlagBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), &zero);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // ==== PASS 2: prefix sum of blockSums → blockOffsets ====
        prefixPass2->use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, inSums);  // Input block sums from previous level
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outOffs); // Output prefix sums of previous level block sums
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, errorFlagBuffer);

        prefixPass2->setUInt("numBlocks", levelSize);

        glDispatchCompute(dispatchCount, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // !!!!!!!! DEBUG CPU IMPLEMENTATION, REPLACE WITH GPU SHADER !!!!!!!!!!
        // This update to levelBlockOffsetsBuffers[lvl] was missing!!!!!
        if (lvl + 1 < numLevels) {
            // Extract the last value of each block from outOffs and store in levelBlockSumsBuffers[lvl + 1]
            std::vector<GLuint> nextLevelSums(dispatchCount, 0);

            glBindBuffer(GL_SHADER_STORAGE_BUFFER, outOffs);
            GLuint* outOffsData = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint) * levelSize, GL_MAP_READ_BIT);
            if (outOffsData) {
                for (int b = 0; b < dispatchCount; ++b) {
                    size_t lastIdx = std::min<size_t>((b + 1) * WORKGROUP_SIZE, static_cast<size_t>(levelSize)) - 1;
                    nextLevelSums[b] = outOffsData[lastIdx];
                }
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            }
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            // Write to next level block sums buffer
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, levelBlockSumsBuffers[lvl + 1]);
            glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, dispatchCount * sizeof(GLuint), nextLevelSums.data());
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        // Optional: error checking
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, errorFlagBuffer);
        GLuint* ptr = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), GL_MAP_READ_BIT);
        if (ptr && *ptr != 0)
            std::cerr << "Error in prefixPass2 at level " << lvl << ": errorFlag = " << *ptr << std::endl;
        if (ptr) glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        //@@@ PASS2 CHECK (shows the block offsets)
        // Expected outOffs: [0, 1024, 2048, ..., xxx] for 1025 values
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, outOffs);
        GLuint* outOffsData = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint) * levelSize, GL_MAP_READ_BIT);
        if (outOffsData) {
            std::cout << "lvl " << lvl << " outOffs from pass2 (size " << levelSize << "):" << std::endl;
            size_t numBlocksToShow = div_ceil(levelSize, WORKGROUP_SIZE);
            size_t blocksToPrint = 3;
            // Print first 3 blocks
            for (size_t block = 0; block < std::min(blocksToPrint, numBlocksToShow); ++block) {
            size_t start = block * WORKGROUP_SIZE;
            size_t end = std::min(start + WORKGROUP_SIZE, (size_t)levelSize);
            std::cout << "Block " << block << " (elements " << start << " to " << end - 1 << "): ";
            size_t blockSize = end - start;
            if (blockSize <= 6) {
                for (size_t i = start; i < end; ++i) {
                if (i > start) std::cout << ", ";
                std::cout << outOffsData[i];
                }
            } else {
                for (size_t i = 0; i < 3; ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << outOffsData[start + i];
                }
                std::cout << ", ..., ";
                for (size_t i = blockSize - 3; i < blockSize; ++i) {
                if (i > blockSize - 3) std::cout << ", ";
                std::cout << outOffsData[start + i];
                }
            }
            std::cout << std::endl;
            }
            if (numBlocksToShow > 2 * blocksToPrint) {
            std::cout << "......" << std::endl;
            }
            // Print last 3 blocks
            for (size_t block = (numBlocksToShow > blocksToPrint ? numBlocksToShow - blocksToPrint : blocksToPrint); block < numBlocksToShow; ++block) {
            size_t idx = block;
            if (numBlocksToShow > 2 * blocksToPrint)
                idx = numBlocksToShow - blocksToPrint + (block - (numBlocksToShow - blocksToPrint));
            if (idx < blocksToPrint) continue; // skip if already printed in first loop
            size_t start = idx * WORKGROUP_SIZE;
            size_t end = std::min(start + WORKGROUP_SIZE, (size_t)levelSize);
            std::cout << "Block " << idx << " (elements " << start << " to " << end - 1 << "): ";
            size_t blockSize = end - start;
            if (blockSize <= 6) {
                for (size_t i = start; i < end; ++i) {
                if (i > start) std::cout << ", ";
                std::cout << outOffsData[i];
                }
            } else {
                for (size_t i = 0; i < 3; ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << outOffsData[start + i];
                }
                std::cout << ", ..., ";
                for (size_t i = blockSize - 3; i < blockSize; ++i) {
                if (i > blockSize - 3) std::cout << ", ";
                std::cout << outOffsData[start + i];
                }
            }
            std::cout << std::endl;
            }
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        //@@@ END PASS2 CHECK

        // ==== PASS 3: add blockOffsets to lower level ====
        // Apply to full prefix sum output
        // prefixPass3->use();
        // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer); // This contains the prefix sums from pass1
        // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outOffs); // This contains the block offsets from pass2
        // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, errorFlagBuffer);
        // prefixPass3->setUInt("numElements", totalElements);

        int lowerLevelSize = levelSizes[lvl - 1];
        int lowerNumBlocks = div_ceil(lowerLevelSize, WORKGROUP_SIZE);

        prefixPass3->use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outOffs); // outOffs is blockOffsets for current level
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, errorFlagBuffer);
        prefixPass3->setUInt("numElements", lowerLevelSize); // not totalElements!

        glDispatchCompute(lowerNumBlocks, 1, 1);
        // glDispatchCompute(numBlocks, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //@@@ PASS3 CHECK (shows the final prefix sum)
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
        GLuint* prefixData = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, (totalElements + 1) * sizeof(GLuint), GL_MAP_READ_BIT);
        if (prefixData) {
            std::cout << "prefixSumBuffer after pass3 (size " << (totalElements + 1) << "):" << std::endl;
            size_t numBlocksToShow = numBlocks;
            size_t blocksToPrint = 3;
            // Print first 3 blocks
            for (size_t block = 0; block < std::min(blocksToPrint, numBlocksToShow); ++block) {
                size_t start = block * WORKGROUP_SIZE;
                size_t end = std::min(start + WORKGROUP_SIZE, totalElements);
                std::cout << "Block " << block << " (elements " << start << " to " << end - 1 << "): ";
                size_t blockSize = end - start;
                if (blockSize <= 6) {
                    for (size_t i = start; i < end; ++i) {
                        if (i > start) std::cout << ", ";
                        std::cout << prefixData[i];
                    }
                } else {
                    // Show first 3 and last 3 elements
                    for (size_t i = 0; i < 3; ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << prefixData[start + i];
                    }
                    std::cout << ", ..., ";
                    for (size_t i = blockSize - 3; i < blockSize; ++i) {
                        if (i > blockSize - 3) std::cout << ", ";
                        std::cout << prefixData[start + i];
                    }
                }
                std::cout << std::endl;
            }
            if (numBlocksToShow > 2 * blocksToPrint) {
                std::cout << "......" << std::endl;
            }
            // Print last 3 blocks
            for (size_t block = (numBlocksToShow > blocksToPrint ? numBlocksToShow - blocksToPrint : blocksToPrint); block < numBlocksToShow; ++block) {
                size_t idx = block;
                if (numBlocksToShow > 2 * blocksToPrint)
                    idx = numBlocksToShow - blocksToPrint + (block - (numBlocksToShow - blocksToPrint));
                if (idx < blocksToPrint) continue; // skip if already printed in first loop
                size_t start = idx * WORKGROUP_SIZE;
                size_t end = std::min(start + WORKGROUP_SIZE, totalElements);
                std::cout << "Block " << idx << " (elements " << start << " to " << end - 1 << "): ";
                size_t blockSize = end - start;
                if (blockSize <= 6) {
                    for (size_t i = start; i < end; ++i) {
                        if (i > start) std::cout << ", ";
                        std::cout << prefixData[i];
                    }
                } else {
                    for (size_t i = 0; i < 3; ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << prefixData[start + i];
                    }
                    std::cout << ", ..., ";
                    for (size_t i = blockSize - 3; i < blockSize; ++i) {
                        if (i > blockSize - 3) std::cout << ", ";
                        std::cout << prefixData[start + i];
                    }
                }
                std::cout << std::endl;
            }
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        //@@@ END PASS3 CHECK

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // Cleanup temporary buffers
    for (int i = 2; i < numLevels; ++i) {
        glDeleteBuffers(1, &levelBlockSumsBuffers[i]);
        glDeleteBuffers(1, &levelBlockOffsetsBuffers[i]);
    }
}
