#pragma once

#include <vector>
#include <cstdint>

class TransitionCompressor {
public:
    TransitionCompressor(uint32_t resolution, uint32_t depth);

    void addSlice(const std::vector<uint8_t>& pixelBuffer);  // Adds a new slice
    size_t getSize() const;                                  // Returns total number of stored values
    const std::vector<uint32_t>& getData() const;            // Access compressed data

private:
    uint32_t resolution;
    uint32_t depth;
    uint32_t currentDepth = 0;
    std::vector<uint32_t> transitions;
};
