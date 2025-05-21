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
#include "shader.hpp"
#include "meshLoader.hpp"
#include "glUtils.hpp"


// OPTIMIZED BY DEEPSEEK
void voxelize(
  const MeshBuffers& mesh,
  int triangleCount,
  float zSpan,
  Shader* shader,
  Shader* transitionShader,
  GLFWwindow* window,
  GLuint fbo,
  GLuint colorTex,
  int resolution,
  bool preview
) {
  glm::mat4 projection = glm::ortho(-0.51f, 0.51f, -0.51f, 0.51f, 0.0f, zSpan);
  glm::mat4 view = glm::lookAt(glm::vec3(0, 0, zSpan / 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
  glm::mat4 model = glm::mat4(1.0f);

  shader->use();
  shader->setMat4("projection", projection);
  shader->setMat4("view", view);
  shader->setMat4("model", model);

  const int MAX_TRANSITIONS_PER_ROW = 32;
  size_t maxTransitionCount = size_t(resolution) * resolution * MAX_TRANSITIONS_PER_ROW;
  size_t totalRows = size_t(resolution) * resolution;

  std::cout << "Transitions SSBO size: " << (maxTransitionCount * sizeof(GLuint)) / (1024.0 * 1024.0) << " MB\n";

  GLuint transitionSSBO;
  glGenBuffers(1, &transitionSSBO);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionSSBO);
  glBufferData(GL_SHADER_STORAGE_BUFFER, maxTransitionCount * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
  if (!checkGLError("TransitionSSBO allocation failed!", transitionSSBO)) return;
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, transitionSSBO);

  GLuint rowCountBuffer;
  glGenBuffers(1, &rowCountBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, rowCountBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalRows * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, rowCountBuffer);

  std::vector<GLuint> zeroRowCounts(totalRows, 0);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalRows * sizeof(GLuint), zeroRowCounts.data());

  // Pre-bind the image texture once
  glBindImageTexture(0, colorTex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

  // Set static uniforms for transitionShader once
  transitionShader->use();
  transitionShader->setInt("resolution", resolution);
  transitionShader->setUInt("maxTransitionCount", maxTransitionCount);
  transitionShader->setUInt("maxTransitionsPerRow", MAX_TRANSITIONS_PER_ROW);
  transitionShader->setUInt("totalRowCount", totalRows);
  shader->use(); // Switch back to main shader

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glViewport(0, 0, resolution, resolution);
  glBindVertexArray(mesh.vao);

  float deltaZ = zSpan / resolution;

  for (int i = 0; i < resolution; ++i) {
      shader->use();
      float z = zSpan / 2.0f - static_cast<float>(i) * deltaZ;
      glm::vec4 clippingPlane(glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)), z);
      shader->setVec4("clippingPlane", clippingPlane);

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      
      glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);
      glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

      if (preview) {
          renderPreview(mesh.vao, triangleCount, window, resolution, resolution);
      }

      transitionShader->use();
      transitionShader->setInt("sliceIndex", i); // Only dynamic uniform

      glDispatchCompute(1, resolution, 1);
      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

      // Invalidate framebuffer to optimize memory usage
      GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT };
      glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, attachments);

      if (i % (resolution / 10) == 0)
          std::cout << "Progress: " << (100 * i / resolution) << "%\n";
  }

  GLuint totalTransitions = sumUIntBuffer(rowCountBuffer, totalRows);
  std::cout << "Total transitions in volume: " << totalTransitions << std::endl;
  checkSentinelAt(transitionSSBO, maxTransitionCount - 1, 0xDEADBEEF);

  //&&& Buffer compression
}


// THIS CODE IS OK, JUST A LITTLE SLOWER THAN THE ONE ABOVE
/*
void voxelize(
    const MeshBuffers& mesh,
    int triangleCount,
    float zSpan,
    Shader* shader,
    Shader* transitionShader,
    GLFWwindow* window,
    GLuint fbo,
    GLuint colorTex,
    int resolution,
    bool preview
) {
    // std::vector<uint8_t> pixelBuffer(resolution * resolution * 4);

    glm::mat4 projection = glm::ortho(-0.51f, 0.51f, -0.51f, 0.51f, 0.0f, zSpan);
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, zSpan / 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 model = glm::mat4(1.0f);

    shader->use();
    shader->setMat4("projection", projection);
    shader->setMat4("view", view);
    shader->setMat4("model", model);

    const int MAX_TRANSITIONS_PER_ROW = 32;
    size_t maxTransitionCount = size_t(resolution) * resolution * MAX_TRANSITIONS_PER_ROW;
    size_t totalRows = size_t(resolution) * resolution;

    std::cout << "Transitions SSBO size: " << (maxTransitionCount * sizeof(GLuint)) / (1024.0 * 1024.0) << " MB\n";

    GLuint transitionSSBO;
    glGenBuffers(1, &transitionSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionSSBO);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, maxTransitionCount * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxTransitionCount * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW); // Better performance if only writing
    if (!checkGLError("TransitionSSBO allocation failed!", transitionSSBO)) return;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, transitionSSBO);

    GLuint rowCountBuffer;
    glGenBuffers(1, &rowCountBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, rowCountBuffer);
    // glBufferData(GL_SHADER_STORAGE_BUFFER, totalRows * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);
    glBufferData(GL_SHADER_STORAGE_BUFFER, totalRows * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW); // Better performance if only writing
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, rowCountBuffer);

    std::vector<GLuint> zeroRowCounts(totalRows, 0);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalRows * sizeof(GLuint), zeroRowCounts.data());

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glViewport(0, 0, resolution, resolution);
    glBindVertexArray(mesh.vao);

    for (int i = 0; i < resolution; ++i) {
        shader->use();
        float z = zSpan / 2.0f - static_cast<float>(i) * zSpan / resolution;
        glm::vec4 clippingPlane(glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)), z);
        shader->setVec4("clippingPlane", clippingPlane);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);
        glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT); //@@@ Maybe not needed

        if (preview) {
            renderPreview(mesh.vao, triangleCount, window, resolution, resolution);
        }

        glBindImageTexture(0, colorTex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

        transitionShader->use();
        transitionShader->setInt("resolution", resolution);
        transitionShader->setInt("sliceIndex", i);
        transitionShader->setUInt("maxTransitionCount", maxTransitionCount);
        transitionShader->setUInt("maxTransitionsPerRow", MAX_TRANSITIONS_PER_ROW);
        transitionShader->setUInt("totalRowCount", totalRows);

        glDispatchCompute(1, resolution, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

        if (i % (resolution / 10) == 0)
            std::cout << "Progress: " << (100 * i / resolution) << "%\n";
    }

    GLuint totalTransitions = sumUIntBuffer(rowCountBuffer, totalRows);
    std::cout << "Total transitions in volume: " << totalTransitions << std::endl;
    checkSentinelAt(transitionSSBO, maxTransitionCount - 1, 0xDEADBEEF);

    //&&& Buffer compression

}
*/