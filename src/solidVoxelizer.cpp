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

void solidVoxelize(const char* stlPath, GLuint& voxelSSBO, GLuint& vertexSSBO, GLuint& indexSSBO, VoxelizerComputeShader* computeShader, const glm::vec3& bboxMin, const glm::vec3& bboxMax) {
  std::vector<float> vertices;
  std::vector<unsigned int> indices;
  float zSpan = 1.01f * loadMesh(stlPath, vertices, indices);
  std::cout << "zSpan: " << zSpan << std::endl;

  const int GRID_RES = 256;
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

  glDispatchCompute((TRIANGLE_COUNT + 63) / 64, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}
