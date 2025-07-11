#include <glad/glad.h> // This before GLFW to avoid conflicts
#include "voxelizerUtils.hpp"

size_t estimateMemoryUsageBytes(const VoxelizationParams& params) {
    size_t totalPixels = size_t(params.resolutionXYZ.x) * size_t(params.resolutionXYZ.y);

    size_t sliceTexBytes = params.resolutionXYZ.x * params.resolutionXYZ.y * (params.slicesPerBlock + 1) * 4; // RGBA8 = 4 bytes
    size_t transitionBufferBytes = totalPixels * params.maxTransitionsPerZColumn * sizeof(GLuint);
    size_t countBufferBytes = totalPixels * sizeof(GLuint);
    size_t overflowBufferBytes = totalPixels * sizeof(GLuint);

    return sliceTexBytes + transitionBufferBytes + countBufferBytes + overflowBufferBytes;
}

int chooseOptimalSlicesPerBlock(const VoxelizationParams& params) {
    int bestSlices = 1;
    for (int testSlices = 1; testSlices <= std::min(128, params.resolutionXYZ.z); ++testSlices) {
        VoxelizationParams testParams = params;
        testParams.slicesPerBlock = testSlices;
        size_t mem = estimateMemoryUsageBytes(testParams);
        if (mem < params.maxMemoryBudgetBytes) {
            bestSlices = testSlices;
        } else {
            break;
        }
    }
    return bestSlices;
}

int chooseOptimalPowerOfTwoSlicesPerBlock(const VoxelizationParams& params) {
    int bestSlices = 1;
    for (int testSlices = 1; testSlices <= std::min(128, params.resolutionXYZ.z); testSlices *= 2) {
        VoxelizationParams testParams = params;
        testParams.slicesPerBlock = testSlices;
        size_t mem = estimateMemoryUsageBytes(testParams);
        if (mem < params.maxMemoryBudgetBytes) {
            bestSlices = testSlices;
        } else {
            break;
        }
    }
    return bestSlices;
}