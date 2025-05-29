#pragma once

#include <string>

// Main function to visualize a voxelized volume using raymarching
void visualizeZ(const std::string& compressedBufferFile,
               const std::string& prefixSumBufferFile,
               int resolutionXY, int resolutionZ,
               int maxTransitionsPerZColumn);
