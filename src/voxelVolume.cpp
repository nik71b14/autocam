#include "VoxelVolume.hpp"

// Constructor: initializes the layers to an empty vector
VoxelVolume::VoxelVolume() {}

// Add a slice of data as a QuadTreeNode
void VoxelVolume::addSlice(const std::vector<uint8_t>& sliceData, size_t resolution) {
    auto root = QuadTreeNode::fromBitMatrix(sliceData, resolution);
    layers.push_back(root);
}

// Get the total size of the volume
size_t VoxelVolume::getTotalSize() const {
    size_t totalSize = 0;
    for (const auto& layer : layers) {
        totalSize += layer->getSize();
    }
    return totalSize;
}

// Estimate memory usage of the volume
size_t VoxelVolume::getEstimatedMemoryUsage(size_t bytesPerNode) const {
    size_t nodeCount = getTotalSize();
    return nodeCount * bytesPerNode;  // in bytes
}

// Carve this volume with another volume (layer by layer)
void VoxelVolume::carveWith(const VoxelVolume& tool) {
    size_t minLayers = std::min(layers.size(), tool.layers.size());
    for (size_t z = 0; z < minLayers; ++z) {
        layers[z]->carve(tool.layers[z]);
    }
}
