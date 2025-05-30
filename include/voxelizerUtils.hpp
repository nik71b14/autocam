#pragma once

#include <cstddef>
#include "voxelizer.hpp"

// Estimate memory usage in bytes for the voxelization process
size_t estimateMemoryUsageBytes(const VoxelizationParams& params);

// Choose the optimal number of slices per block under the memory budget
int chooseOptimalSlicesPerBlock(const VoxelizationParams& params);

// Choose the optimal power-of-two number of slices per block under the memory budget
int chooseOptimalPowerOfTwoSlicesPerBlock(const VoxelizationParams& params);
