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

#define FLUSH_INTERVAL 50

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
  size_t totalRows = size_t(resolution) * resolution;
  size_t maxTransitionCount = totalRows * MAX_TRANSITIONS_PER_ROW;
  

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
  // glBufferData(GL_SHADER_STORAGE_BUFFER, totalRows * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalRows * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);
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
  static const glm::vec3 planeNormal(0.0f, 0.0f, -1.0f);

  for (int i = 0; i < resolution; ++i) {
      shader->use();
      float z = zSpan / 2.0f - static_cast<float>(i) * deltaZ;
      glm::vec4 clippingPlane(planeNormal, z);
      shader->setVec4("clippingPlane", clippingPlane);

      glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      
      glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);
      if (!checkGLError("Drawing failed")) return;

      glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);

      if (preview) {
          renderPreview(mesh.vao, triangleCount, window, resolution, resolution);
      }

      transitionShader->use();
      transitionShader->setInt("sliceIndex", i); // This is a dynamic uniform

      //glDispatchCompute(1, resolution, 1);
      const int WG_SIZE = 32; // Most GPUs need 32-64 threads per workgroup
      glDispatchCompute(1, (resolution + WG_SIZE-1)/WG_SIZE, 1);
      if (!checkGLError("Compute failed")) return;

      glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

      // Invalidate framebuffer to optimize memory usage
      GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT };
      glInvalidateFramebuffer(GL_FRAMEBUFFER, 2, attachments);

      #ifdef FLUSH_INTERVAL
        if (i % FLUSH_INTERVAL == 0) glFlush(); // Partial synchronization every 10 slices, prevents stalling the GPU
      #endif

      if (i % (resolution / 10) == 0 || i == resolution - 1) {
        int percent = static_cast<int>((100.0f * i) / (resolution - 1));
        std::cout << "\rGPU progress: " << std::setw(3) << percent << "%" << std::flush;
        if (percent == 100) std::cout << std::endl;
      }
  }

  glFinish(); // Ensure all commands are completed before reading back data

  // Check for errors after the compute shader dispatch
  if (!checkSentinelAt(transitionSSBO, maxTransitionCount - 1, 0xDEADBEEF)) {
      std::cerr << "Sentinel check failed!" << std::endl;
      GLuint buffers[] = {transitionSSBO, rowCountBuffer};
      glDeleteBuffers(2, buffers);
      return;
  }

  // Read back the total number of transitions
  GLuint totalTransitions = sumUIntBuffer(rowCountBuffer, totalRows);
  std::cout << "Total transitions in volume: " << totalTransitions << std::endl;

  // Data extraction and compression
  // - transitionSSBO is a 1D array of size totalRows * MAX_TRANSITIONS_PER_ROW,
  // meaning each row has a fixed block of MAX_TRANSITIONS_PER_ROW reserved elements.
  // - rowCountBuffer[row] indicates how many of these elements are actually valid
  // and have been written for that row.
  // - The remaining values in the block for that row (up to MAX_TRANSITIONS_PER_ROW)
  // can be ignored / are garbage / placeholders.

  // 1. Read the row counts from the rowCountBuffer
  std::vector<GLuint> rowCounts(totalRows);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, rowCountBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalRows * sizeof(GLuint), rowCounts.data());

  // 2. Read the transition data from the transitionSSBO
  std::vector<GLuint> transitionData(totalRows * MAX_TRANSITIONS_PER_ROW);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionSSBO);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, transitionData.size() * sizeof(GLuint), transitionData.data());

  // 3. Create compacted buffer
  std::vector<GLuint> compactTransitions;
  compactTransitions.reserve(totalTransitions);

  // 4. Compact data from transitionData into compactTransitions
  for (size_t row = 0; row < totalRows; ++row) {
      GLuint count = rowCounts[row];
      size_t baseIndex = row * MAX_TRANSITIONS_PER_ROW;
      for (GLuint i = 0; i < count; ++i) {
          compactTransitions.push_back(transitionData[baseIndex + i]);
      }
  }

  // Ora compactTransitions contiene solo i valori validi, uno dietro l'altro
  std::cout << "Compacted transitions: " 
            << compactTransitions.size() << " (" 
            << (compactTransitions.size() * sizeof(GLuint)) / (1024.0 * 1024.0) 
            << " MB)" << std::endl;

  // Cleanup
  GLuint buffers[] = {transitionSSBO, rowCountBuffer};
  glDeleteBuffers(2, buffers);
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