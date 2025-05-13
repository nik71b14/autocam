#ifndef QUADTREE_HPP
#define QUADTREE_HPP

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <algorithm>

class FlatQuadTreeBuilder {
public:
    FlatQuadTreeBuilder(std::vector<uint32_t>& data, uint32_t offset, uint32_t size);

    uint32_t build(const std::vector<uint8_t>& matrix);
    uint32_t getNextIndex() const;

private:
    std::vector<uint32_t>& data;
    uint32_t offset;
    uint32_t size;
    uint32_t nextIndex;

    uint32_t buildRecursive(const std::vector<uint8_t>& matrix, uint32_t size, uint32_t x, uint32_t y, uint32_t span);
};

class QuadtreeVolume {
public:
    QuadtreeVolume(uint32_t resolution, uint32_t depth);

    void addSlice(uint32_t sliceIndex, const std::vector<uint8_t>& matrix);
    void finalize();

    size_t getDataSize() const;
    const std::vector<uint32_t>& getData() const;
    const std::vector<uint32_t>& getSliceOffsets() const;

private:
    uint32_t resolution;
    uint32_t depth;
    std::vector<uint32_t> sliceOffsets;
    std::vector<uint32_t> dynamicData;
    std::vector<uint32_t> quadtreeData;
    uint32_t dataIndex;
};

#endif // QUADTREE_HPP
