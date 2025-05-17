#include "GLUtils.hpp"
#include <limits>
#include <iostream>
#include <algorithm>
#include <glm/glm.hpp>
#include <vector>

int logSSBOSize(GLuint ssbo) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    GLint sizeInBytes = 0;
    glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &sizeInBytes);
    //std::cout << "Size of " << name << ": " << sizeInBytes / (1024.0 * 1024.0) << " MB\n";
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return sizeInBytes;
}

std::pair<glm::vec3, glm::vec3> computeBoundingBox(const std::vector<float>& vertices) {
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    for (size_t i = 0; i < vertices.size(); i += 3) {
        minX = std::min(minX, vertices[i]);
        minY = std::min(minY, vertices[i + 1]);
        minZ = std::min(minZ, vertices[i + 2]);
        maxX = std::max(maxX, vertices[i]);
        maxY = std::max(maxY, vertices[i + 1]);
        maxZ = std::max(maxZ, vertices[i + 2]);
    }

    glm::vec3 min(minX, minY, minZ);
    glm::vec3 max(maxX, maxY, maxZ);

    // std::cout << "Min coordinates: (" << min.x << ", " << min.y << ", " << min.z << ")\n";
    // std::cout << "Max coordinates: (" << max.x << ", " << max.y << ", " << max.z << ")\n";

    return {min, max};
}

std::pair<glm::vec3, glm::vec3> computeBoundingBoxVec3(const std::vector<glm::vec3>& vertices) {
    if (vertices.empty()) {
        return { glm::vec3(0.0f), glm::vec3(0.0f) };
    }

    glm::vec3 minCoords(std::numeric_limits<float>::max());
    glm::vec3 maxCoords(std::numeric_limits<float>::lowest());

    for (const auto& v : vertices) {
        minCoords.x = std::min(minCoords.x, v.x);
        minCoords.y = std::min(minCoords.y, v.y);
        minCoords.z = std::min(minCoords.z, v.z);

        maxCoords.x = std::max(maxCoords.x, v.x);
        maxCoords.y = std::max(maxCoords.y, v.y);
        maxCoords.z = std::max(maxCoords.z, v.z);
    }

    return {minCoords, maxCoords};
}

#include <iostream>
#include <glad/glad.h>

void queryGPULimits() {
    GLint maxSSBOSize = 0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxSSBOSize);

    GLint maxSSBOBindings = 0;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxSSBOBindings);

    GLint maxComputeWorkGroupCount[3] = {0};
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &maxComputeWorkGroupCount[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &maxComputeWorkGroupCount[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &maxComputeWorkGroupCount[2]);

    GLint maxComputeWorkGroupSize[3] = {0};
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &maxComputeWorkGroupSize[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &maxComputeWorkGroupSize[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &maxComputeWorkGroupSize[2]);

    GLint maxComputeWorkGroupInvocations = 0;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxComputeWorkGroupInvocations);

    std::cout << "=========================================\n";
    std::cout << "|           GPU Limits Overview         |\n";
    std::cout << "=========================================\n";
    std::cout << "| Max SSBO size (MB):\n";
    std::cout << "|   " << maxSSBOSize / (1024.0 * 1024.0) << " MB\n";
    std::cout << "|---------------------------------------|\n";
    std::cout << "| Max SSBO bindings:\n";
    std::cout << "|   " << maxSSBOBindings << "\n";
    std::cout << "|---------------------------------------|\n";
    std::cout << "| Max compute work group count:         |\n";
    std::cout << "|   X: " << maxComputeWorkGroupCount[0] << "\n";
    std::cout << "|   Y: " << maxComputeWorkGroupCount[1] << "\n";
    std::cout << "|   Z: " << maxComputeWorkGroupCount[2] << "\n";
    std::cout << "|---------------------------------------|\n";
    std::cout << "| Max compute work group size:          |\n";
    std::cout << "|   X: " << maxComputeWorkGroupSize[0] << "\n";
    std::cout << "|   Y: " << maxComputeWorkGroupSize[1] << "\n";
    std::cout << "|   Z: " << maxComputeWorkGroupSize[2] << "\n";
    std::cout << "|---------------------------------------|\n";
    std::cout << "| Max compute work group invocations:   |\n";
    std::cout << "|   " << maxComputeWorkGroupInvocations << "\n";
    std::cout << "=========================================\n";
}
