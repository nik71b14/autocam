#include "quadtreeFlat.hpp"
#include "quadtreeNode.hpp"

// === FlatQuadTreeBuilder ===
FlatQuadTreeBuilder::FlatQuadTreeBuilder(std::vector<uint32_t>& data, uint32_t offset, uint32_t size)
    : data(data), offset(offset), size(size), nextIndex(0) {}

uint32_t FlatQuadTreeBuilder::build(const std::vector<uint8_t>& matrix) {
    nextIndex = 0;
    return buildRecursive(matrix, size, 0, 0, size);
}

uint32_t FlatQuadTreeBuilder::getNextIndex() const {
    return offset + nextIndex;
}

uint32_t FlatQuadTreeBuilder::buildRecursive(const std::vector<uint8_t>& matrix, uint32_t size, uint32_t x, uint32_t y, uint32_t span) {
    // if (span == 1) {
    //     uint8_t pixel = matrix[y * size + x];
    //     uint32_t index = offset + nextIndex++;
    //     if (data.size() <= index) data.resize(index + 1);
    //     data[index] = pixel ? 1 : 0;
    //     return index;
    // }

    if (span == 1) {
        size_t bitIndex = y * size + x;
        bool pixel = QuadTreeNode::getBit(matrix, bitIndex);  // Safely fetch the bit // @@@ MOVE getBit in a helper class???
        uint32_t index = offset + nextIndex++;
        if (data.size() <= index) data.resize(index + 1);
        data[index] = pixel ? 1 : 0;
        return index;
    }

    uint32_t currentNextIndex = nextIndex;
    uint32_t half = span / 2;

    std::vector<uint32_t> children = {
        buildRecursive(matrix, size, x, y, half),
        buildRecursive(matrix, size, x + half, y, half),
        buildRecursive(matrix, size, x, y + half, half),
        buildRecursive(matrix, size, x + half, y + half, half)
    };

    std::vector<uint32_t> values(4);
    for (size_t i = 0; i < 4; ++i) values[i] = data[children[i]];
    bool allSame = std::all_of(values.begin(), values.end(), [&](uint32_t v) { return v == values[0]; }) && values[0] != 2;

    if (allSame) {
        nextIndex = currentNextIndex;
        uint32_t index = offset + nextIndex++;
        if (data.size() <= index) data.resize(index + 1);
        data[index] = values[0];
        return index;
    } else {
        uint32_t parentIndex = offset + nextIndex++;
        if (data.size() <= parentIndex + 4) data.resize(parentIndex + 5);
        data[parentIndex] = 2;
        for (const auto& child : children) {
            data[offset + nextIndex++] = child;
        }
        return parentIndex;
    }
}

// === QuadtreeVolume ===
QuadtreeVolume::QuadtreeVolume(uint32_t resolution, uint32_t depth)
    : resolution(resolution), depth(depth), sliceOffsets(depth), dataIndex(0) {}

void QuadtreeVolume::addSlice(uint32_t sliceIndex, const std::vector<uint8_t>& matrix) {

    FlatQuadTreeBuilder builder(dynamicData, dataIndex, resolution);
    uint32_t start = builder.build(matrix);
    if (sliceIndex >= sliceOffsets.size())
        throw std::out_of_range("sliceIndex out of range");
    sliceOffsets[sliceIndex] = start;
    dataIndex = builder.getNextIndex();
}

void QuadtreeVolume::finalize() {
    quadtreeData = std::vector<uint32_t>(dynamicData.begin(), dynamicData.end());
    dynamicData.clear();
}

size_t QuadtreeVolume::getDataSize() const {
    return quadtreeData.size() * sizeof(uint32_t);
}

const std::vector<uint32_t>& QuadtreeVolume::getData() const {
    return quadtreeData;
}

const std::vector<uint32_t>& QuadtreeVolume::getSliceOffsets() const {
    return sliceOffsets;
}
