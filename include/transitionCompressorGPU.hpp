#ifndef TRANSITIONCOMPRESSORGPU_H
#define TRANSITIONCOMPRESSORGPU_H

#include <glad/glad.h>
#include <vector>
#include <stdexcept>

class TransitionCompressorGPU {
public:
    TransitionCompressorGPU(GLuint transitionSSBO, GLuint counterSSBO);
    ~TransitionCompressorGPU();

    // Method to add a transition slice from GPU-side buffer (after compute shader dispatch)
    void addTransitionSlice(const std::vector<uint32_t>& transitions);

    // Get the size of the stored transitions
    size_t getSize() const;

    // Get the transition data
    const std::vector<uint32_t>& getData() const;

    // Reset the stored transitions (for new frame or batch)
    void reset();

private:
    GLuint transitionSSBO;  // Shader storage buffer for storing transitions
    GLuint counterSSBO;     // Shader storage buffer for storing counter (atomic operations)
    
    size_t size;            // Number of transitions (for CPU-side usage)
    std::vector<uint32_t> transitions;  // A vector to store transitions retrieved from GPU
};

#endif
