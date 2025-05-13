#include "TransitionCompressorGPU.hpp"
#include <iostream>

TransitionCompressorGPU::TransitionCompressorGPU(GLuint transitionSSBO, GLuint counterSSBO)
    : transitionSSBO(transitionSSBO), counterSSBO(counterSSBO), size(0) {}

TransitionCompressorGPU::~TransitionCompressorGPU() {
    // Clean up the resources
    glDeleteBuffers(1, &transitionSSBO);
    glDeleteBuffers(1, &counterSSBO);
}

void TransitionCompressorGPU::addTransitionSlice(const std::vector<uint32_t>& transitionsSlice) {
    // Append the transitions slice to the local transitions vector
    transitions.insert(transitions.end(), transitionsSlice.begin(), transitionsSlice.end());
    size += transitionsSlice.size();
}

size_t TransitionCompressorGPU::getSize() const {
    return size;
}

const std::vector<uint32_t>& TransitionCompressorGPU::getData() const {
    return transitions;
}

void TransitionCompressorGPU::reset() {
    // Reset the size and clear the transitions vector
    size = 0;
    transitions.clear();
}
