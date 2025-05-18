#include <iostream>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <thread>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "voxelizerComputeShader.hpp"
#include "meshLoader.hpp"
#include "glUtils.hpp"

void solidVoxelize(const char* stlPath, GLuint& voxelSSBO, GLuint& vertexSSBO, GLuint& indexSSBO, VoxelizerComputeShader* computeShader, const glm::vec3& bboxMin, const glm::vec3& bboxMax) {
  std::vector<glm::vec3> vertices;  // changed here
  std::vector<unsigned int> indices;
  float zSpan = 1.01f * loadMeshVec3(stlPath, vertices, indices);  // changed here
  std::cout << "zSpan: " << zSpan << std::endl;

  // If your computeBoundingBox expects std::vector<float>, you need to adapt it to accept std::vector<glm::vec3>
  auto [bbMin, bbMax] = computeBoundingBoxVec3(vertices);
  std::cout << "Min scaled coordinates: (" << bbMin.x << ", " << bbMin.y << ", " << bbMin.z << ")\n";
  std::cout << "Max scaled coordinates: (" << bbMax.x << ", " << bbMax.y << ", " << bbMax.z << ")\n";

  const int GRID_RES = 1024;
  const int TRIANGLE_COUNT = indices.size() / 3;
  const size_t TOTAL_VOXELS = GRID_RES * GRID_RES * GRID_RES;
  const size_t WORD_COUNT = (TOTAL_VOXELS + 31) / 32;

  glGenBuffers(1, &voxelSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, voxelSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, WORD_COUNT * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, voxelSSBO);

  glGenBuffers(1, &vertexSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vertexSSBO);

  glGenBuffers(1, &indexSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indexSSBO);

  computeShader->use();
  computeShader->setInt("gridRes", GRID_RES);
  computeShader->setInt("triangleCount", TRIANGLE_COUNT);
  computeShader->setVec3("bboxMin", bboxMin);
  computeShader->setVec3("bboxMax", bboxMax);

  glDispatchCompute((TRIANGLE_COUNT + 63) / 64, 1, 1); // Rounds up to the nearest multiple of 64 without using floating point division
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // Ensure all writes to the SSBO are finished before reading
}

// Naive voxelization with transitions
// void solidVoxelizeTransition(
//   const char* stlPath,
//   GLuint& transitionsSSBO,
//   GLuint& vertexSSBO,
//   GLuint& indexSSBO,
//   VoxelizerComputeShader* computeShader,
//   const glm::vec3& bboxMin,
//   const glm::vec3& bboxMax)
// {
//     std::vector<glm::vec3> vertices;
//     std::vector<unsigned int> indices;
//     float zSpan = 1.01f * loadMeshVec3(stlPath, vertices, indices);
//     std::cout << "zSpan: " << zSpan << std::endl;

//     auto [bbMin, bbMax] = computeBoundingBoxVec3(vertices);
//     std::cout << "Min scaled coordinates: (" << bbMin.x << ", " << bbMin.y << ", " << bbMin.z << ")\n";
//     std::cout << "Max scaled coordinates: (" << bbMax.x << ", " << bbMax.y << ", " << bbMax.z << ")\n";

//     const int GRID_RES = 1024;
//     const int TRIANGLE_COUNT = indices.size() / 3;
//     const int MAX_TRANSITIONS_PER_COLUMN = 32; // tune this

//     // Buffer size for transitions: gridRes * gridRes * maxTransitions * uint
//     size_t transitionsCount = GRID_RES * GRID_RES * MAX_TRANSITIONS_PER_COLUMN;

//     glGenBuffers(1, &transitionsSSBO);
//     glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionsSSBO);
//     glBufferData(GL_SHADER_STORAGE_BUFFER, transitionsCount * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
//     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transitionsSSBO);

//     glGenBuffers(1, &vertexSSBO);
//     glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexSSBO);
//     glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
//     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, vertexSSBO);

//     glGenBuffers(1, &indexSSBO);
//     glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexSSBO);
//     glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
//     glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, indexSSBO);

//     computeShader->use();
//     computeShader->setInt("gridRes", GRID_RES);
//     computeShader->setInt("triangleCount", TRIANGLE_COUNT);
//     computeShader->setInt("maxTransitions", MAX_TRANSITIONS_PER_COLUMN);
//     computeShader->setVec3("bboxMin", bboxMin);
//     computeShader->setVec3("bboxMax", bboxMax);

//     glDispatchCompute((TRIANGLE_COUNT + 63) / 64, 1, 1);
//     glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
// }


// Z-transition compressed representation using atomic counters. This approach stores the list of Z "enter/exit" transitions
// per XY column and is far more compact for solid objects.

// Transition structure — must match compute shader
struct Transition {
    float z;            // Z value of transition
    uint32_t x, y;      // X-Y grid coordinate
    uint32_t enter;     // 1 = enter, 0 = exit
};

GLuint resizeTransitionBufferGPU(GLuint originalBuffer, GLuint counterBuffer); // forward declaration

void solidVoxelizeTransition(
    const char* stlPath,
    GLuint& transitionsSSBO,
    GLuint& counterSSBO,
    GLuint& vertexSSBO,
    GLuint& indexSSBO,
    VoxelizerComputeShader* computeShader,
    const glm::vec3& bboxMin,
    const glm::vec3& bboxMax
) {
    // === Load mesh data from STL ===
    std::vector<glm::vec3> vertices;
    std::vector<unsigned int> indices;
    loadMeshVec3(stlPath, vertices, indices);

    const int GRID_RES = 1024; // Resolution of voxel grid along each axis
    const int TRIANGLE_COUNT = indices.size() / 3;

    // Conservative max transition count: each triangle could, in worst case, affect multiple XY columns
    const size_t MAX_TRANSITIONS = GRID_RES * GRID_RES * 64;

    // === Allocate GPU buffer for transition list (struct: float z, uint x, uint y, uint enter) ===
    glGenBuffers(1, &transitionsSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionsSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_TRANSITIONS * sizeof(Transition), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, transitionsSSBO);  // Binding = 2 in shader

    // === Atomic counter to track how many transitions are written ===
    uint32_t zero = 0;
    glGenBuffers(1, &counterSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t), &zero, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, counterSSBO);  // Binding = 3 in shader

    // === Vertex buffer (vec3) ===
    glGenBuffers(1, &vertexSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, vertexSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, vertexSSBO);  // Binding = 0 in shader

    // === Index buffer (uint) ===
    glGenBuffers(1, &indexSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, indexSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indexSSBO);  // Binding = 1 in shader

    // === Setup shader and uniforms ===
    computeShader->use();
    computeShader->setInt("gridRes", GRID_RES);
    computeShader->setInt("triangleCount", TRIANGLE_COUNT);
    computeShader->setVec3("bboxMin", bboxMin);
    computeShader->setVec3("bboxMax", bboxMax);

    // === Clear atomic counter before dispatch ===
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &zero);

    // === Dispatch GPU compute shader (1 group per 64 triangles) ===
    glDispatchCompute((TRIANGLE_COUNT + 63) / 64, 1, 1);

    // === Ensure compute shader finishes writing before reading data back ===
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // === (Optional) Read back how many transitions were written ===
    // glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterSSBO);
    // uint32_t* counterPtr = (uint32_t*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), GL_MAP_READ_BIT);
    // uint32_t transitionCount = *counterPtr;
    // glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    // std::cout << "Total transitions written: " << transitionCount << std::endl;
    
    // === Resize transition buffer to only the used transitions ===
    // Resize transitionsSSBO to minimal size (reuse or replace the old buffer)
    GLuint smallerTransitionsSSBO = resizeTransitionBufferGPU(transitionsSSBO, counterSSBO);

    if (smallerTransitionsSSBO != 0) {
        // Delete old big buffer
        glDeleteBuffers(1, &transitionsSSBO);

        // Replace with smaller buffer handle
        transitionsSSBO = smallerTransitionsSSBO;

        // Bind the resized smaller buffer to the same binding point for further use
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, transitionsSSBO);
    }

    //@@@ === >Remember to re-bind the smaller buffer to the same binding point (2) if you want to continue using it in subsequent GPU passes.

}

GLuint resizeTransitionBufferGPU(GLuint originalBuffer, GLuint counterBuffer) {
  // Read the number of transitions written from atomic counter (GPU->CPU read)
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterBuffer);
  uint32_t* counterPtr = (uint32_t*)glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), GL_MAP_READ_BIT);
  if (!counterPtr) {
      std::cerr << "Failed to map counter buffer." << std::endl;
      return 0;
  }
  uint32_t transitionCount = *counterPtr;
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

  std::cout << "Transition count after voxelization: " << transitionCount << std::endl;

  if (transitionCount == 0) {
      // No transitions, return 0 or handle appropriately
      return 0;
  }

  // Create a new smaller buffer to hold only the used transitions
  GLuint smallerBuffer;
  glGenBuffers(1, &smallerBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, smallerBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, transitionCount * sizeof(Transition), nullptr, GL_DYNAMIC_DRAW);

  // Copy relevant data from the large original buffer to the smaller one (GPU copy)
  glBindBuffer(GL_COPY_READ_BUFFER, originalBuffer);
  glBindBuffer(GL_COPY_WRITE_BUFFER, smallerBuffer);
  glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, transitionCount * sizeof(Transition));

  // Unbind buffers
  glBindBuffer(GL_COPY_READ_BUFFER, 0);
  glBindBuffer(GL_COPY_WRITE_BUFFER, 0);

  return smallerBuffer;
}

