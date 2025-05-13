#include "TransitionCompressor.hpp"
#include <stdexcept>

TransitionCompressor::TransitionCompressor(uint32_t resolution, uint32_t depth)
    : resolution(resolution), depth(depth) {
    transitions.reserve(depth * resolution);  // Preallocate rough space
}

void TransitionCompressor::addSlice(const std::vector<uint8_t>& pixelBuffer) {
    if (pixelBuffer.size() != resolution * resolution * 4) {
        throw std::runtime_error("Pixel buffer has incorrect size");
    }

    for (uint32_t y = 0; y < resolution; ++y) {
        transitions.push_back(0);  // Row start marker

        bool previous = false;  // Assume black at start
        for (uint32_t x = 0; x < resolution; ++x) {
            uint32_t index = (y * resolution + x) * 4;
            bool isRed = pixelBuffer[index] > 128;  // Only use red channel

            if (isRed != previous) {
                transitions.push_back(x);  // Transition point
                previous = isRed;
            }
        }
    }

    ++currentDepth;
    if (currentDepth > depth) {
        throw std::runtime_error("Exceeded maximum number of slices");
    }
}

size_t TransitionCompressor::getSize() const {
    return transitions.size();
}

const std::vector<uint32_t>& TransitionCompressor::getData() const {
    return transitions;
}
