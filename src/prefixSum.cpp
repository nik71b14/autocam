#include "prefixSum.hpp"

#include <glad/glad.h>
#include <vector>
#include <iostream>
//#include <GL/gl3w.h> // or your GL loader
#include <cassert>
#include "shader.hpp"
#include <algorithm>
#include <iomanip>

//#define DEBUG_PREFIX_SUM

void printBufferContents(GLuint buffer, size_t wg_size, size_t numElements, const std::string& message) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    GLuint* data = (GLuint*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint) * numElements, GL_MAP_READ_BIT);
    
    if (data) {
        std::cout << message << " (size " << numElements << "):" << std::endl;
        size_t numBlocksToShow = div_ceil(numElements, wg_size);
        size_t blocksToPrint = 3;

        // Print first 3 blocks
        for (size_t block = 0; block < std::min(blocksToPrint, numBlocksToShow); ++block) {
            size_t start = block * wg_size;
            size_t end = std::min(start + wg_size, numElements);
            std::cout << "Block " << block << " (elements " << start << " to " << end - 1 << "): ";

            size_t blockSize = end - start;
            if (blockSize <= 6) {
                for (size_t i = start; i < end; ++i) {
                    if (i > start) std::cout << ", ";
                    std::cout << data[i];
                }
            } else {
                for (size_t i = 0; i < 3; ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << data[start + i];
                }
                std::cout << ", ..., ";
                for (size_t i = blockSize - 3; i < blockSize; ++i) {
                    if (i > blockSize - 3) std::cout << ", ";
                    std::cout << data[start + i];
                }
            }
            std::cout << std::endl;
        }

        if (numBlocksToShow > 2 * blocksToPrint) {
            std::cout << "......" << std::endl;
        }

        // Print last 3 blocks
        for (size_t block = (numBlocksToShow > blocksToPrint ? numBlocksToShow - blocksToPrint : blocksToPrint); 
             block < numBlocksToShow; ++block) {

            size_t idx = block;
            if (numBlocksToShow > 2 * blocksToPrint)
                idx = numBlocksToShow - blocksToPrint + (block - (numBlocksToShow - blocksToPrint));

            if (idx < blocksToPrint) continue; // avoid reprinting

            size_t start = idx * wg_size;
            size_t end = std::min(start + wg_size, numElements);
            std::cout << "Block " << idx << " (elements " << start << " to " << end - 1 << "): ";

            size_t blockSize = end - start;
            if (blockSize <= 6) {
                for (size_t i = start; i < end; ++i) {
                    if (i > start) std::cout << ", ";
                    std::cout << data[i];
                }
            } else {
                for (size_t i = 0; i < 3; ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << data[start + i];
                }
                std::cout << ", ..., ";
                for (size_t i = blockSize - 3; i < blockSize; ++i) {
                    if (i > blockSize - 3) std::cout << ", ";
                    std::cout << data[start + i];
                }
            }
            std::cout << std::endl;
        }

        std::cout << std::endl;

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    } else {
        std::cerr << "Failed to map buffer for reading." << std::endl;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}
void printBufferGraph(GLuint buffer, size_t bufferSize, int numRowsToPrint, char symbol = '*') {
    if (bufferSize < 2 || numRowsToPrint < 1) {
        std::cerr << "Invalid buffer size or number of rows." << std::endl;
        return;
    }

    std::vector<GLuint> data(bufferSize);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bufferSize * sizeof(GLuint), data.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Ignore the last element
    size_t dataSize = bufferSize - 1;
    if (dataSize == 0) return;

    GLuint maxVal = *std::max_element(data.begin(), data.begin() + dataSize);
    int maxSymbols = 40;
    double scale = (maxVal > 0) ? static_cast<double>(maxVal) / maxSymbols : 1.0;

    // Find width for index padding
    size_t maxIdx = (dataSize > 0) ? dataSize - 1 : 0;
    int idxWidth = static_cast<int>(std::to_string(maxIdx).size());

    std::cout << "Graphical buffer representation (max " << maxSymbols << " '" << symbol << "' symbols):\n";

    for (int i = 0; i < numRowsToPrint; ++i) {
        size_t idx = (i == numRowsToPrint - 1) ? dataSize - 1 : (dataSize - 1) * i / (numRowsToPrint - 1);
        int symbols = static_cast<int>(data[idx] / scale);
        if (symbols > maxSymbols) symbols = maxSymbols;

        std::cout << "[" << std::setw(idxWidth) << idx << "] ";
        // Left align: print bar first, then value
        for (int j = 0; j < symbols; ++j) std::cout << symbol;
        std::cout << " (" << data[idx] << ")\n";
    }
}


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
    size_t totalElements,
    int WORKGROUP_SIZE = 1024
) {
    //const int WORKGROUP_SIZE = 1024;
    GLuint numBlocks = div_ceil(totalElements, WORKGROUP_SIZE);

    #ifdef DEBUG_PREFIX_SUM
    std::cout << "numBlocks = " << numBlocks << std::endl;
    #endif

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

    #ifdef DEBUG_PREFIX_SUM
    printBufferContents(prefixSumBuffer, WORKGROUP_SIZE, totalElements + 1, "Level 0 prefix sum after pass1");
    #endif

    // LEVELS ≥ 1: only if we have more than WORKGROUP_SIZE elements
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

        prefixPass2->use();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, inSums);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, outOffs);

        GLuint nextLevelSums = (lvl + 1 < numLevels) ? levelBlockSumsBuffers[lvl + 1] : 0; // or dummy buffer if no next level
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, nextLevelSums);

        prefixPass2->setUInt("numBlocks", levelSize);

        glDispatchCompute(dispatchCount, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    #ifdef DEBUG_PREFIX_SUM
    //Output all levelBlockOffsetsBuffers
    for (int i = 1; i < numLevels; ++i) {
        printBufferContents(levelBlockOffsetsBuffers[i], WORKGROUP_SIZE, levelSizes[i], "levelBlockOffsetsBuffers[" + std::to_string(i) + "]");
    }
    #endif

    // Final pass: adjust previous level blockSums with upper-level blockOffsets
    for (int lvl = numLevels - 1; lvl >= 1; lvl--) {
        int lowerLevelSize = levelSizes[lvl - 1];
        int lowerNumBlocks = div_ceil(lowerLevelSize, WORKGROUP_SIZE);

        #ifdef DEBUG_PREFIX_SUM
        if (lvl - 1 >= 1) {
            printBufferContents(levelBlockOffsetsBuffers[lvl - 1], WORKGROUP_SIZE, levelSizes[lvl - 1], "BEFORE levelBlockOffsetsBuffers[" + std::to_string(lvl - 1) + "]");
        }
        #endif
        
        prefixPass3->use();
        if (lvl == 1) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);
        } else {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, levelBlockOffsetsBuffers[lvl - 1]);
        }
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, levelBlockOffsetsBuffers[lvl]);
        prefixPass3->setUInt("numElements", lowerLevelSize);

        glDispatchCompute(lowerNumBlocks, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        #ifdef DEBUG_PREFIX_SUM
        if (lvl - 1 >= 1) {
            printBufferContents(levelBlockOffsetsBuffers[lvl - 1], WORKGROUP_SIZE, levelSizes[lvl - 1], "AFTER levelBlockOffsetsBuffers[" + std::to_string(lvl - 1) + "]");
        }
        #endif

    } 

    // Cleanup temporary buffers
    for (int i = 2; i < numLevels; ++i) {
        glDeleteBuffers(1, &levelBlockSumsBuffers[i]);
        glDeleteBuffers(1, &levelBlockOffsetsBuffers[i]);
    }
}
