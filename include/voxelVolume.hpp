#ifndef VOXEL_VOLUME_HPP
#define VOXEL_VOLUME_HPP

#include "QuadTreeNode.hpp"
#include <vector>
#include <memory>

class VoxelVolume {
public:
    std::vector<std::shared_ptr<QuadTreeNode>> layers;

    // Constructor
    VoxelVolume();

    // Add a slice of data as a QuadTreeNode
    void addSlice(const std::vector<uint8_t>& sliceData, size_t resolution);

    // Get the total size of the volume
    size_t getTotalSize() const;

    // Estimate memory usage
    size_t getEstimatedMemoryUsage(size_t bytesPerNode = 24) const;

    // Carve this volume with another volume (layer by layer)
    void carveWith(const VoxelVolume& tool);
};

#endif // VOXEL_VOLUME_HPP
