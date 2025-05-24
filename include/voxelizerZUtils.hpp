// voxelizerZUtils.hpp

#pragma once

#include <cstddef>

// Parameters for voxelization
struct VoxelizationParams {
  int resolution = 512;
  int resolutionZ = 512; // Default Z resolution
  int slicesPerBlock = 32;
  size_t maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB
  int maxTransitionsPerZColumn = 32;
  bool preview = false; // Whether to render a preview during voxelization
};

// Estimate memory usage in bytes for the voxelization process
size_t estimateMemoryUsageBytes(const VoxelizationParams& params);

// Choose the optimal number of slices per block under the memory budget
int chooseOptimalSlicesPerBlock(const VoxelizationParams& params);

// Choose the optimal power-of-two number of slices per block under the memory budget
int chooseOptimalPowerOfTwoSlicesPerBlock(const VoxelizationParams& params);
