#include "voxelizerZ.hpp"

#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "shader.hpp"
#include "GLUtils.hpp"
#include <thread>
#include "voxelizerZUtils.hpp"

// - Mesh is sliced and rendered into a 3D texture
// - Adjacent slices are compared in a compute shader
// - Transitions are detected (red ↔ black)
// - Per-column transition Z-values are stored in a GPU buffer
// - Count of transitions per XY location is tracked

// After that, i can pack data into an octree, bit compress it, or visualize in a raymarching shader.

const int MAX_TRANSITIONS_PER_Z_COLUMN = 32;

void voxelizeZ(
  const MeshBuffers& mesh,
  int triangleCount,
  float zSpan,
  Shader* drawShader,
  Shader* computeShader,
  GLuint fbo,
  GLuint sliceTex,
  const VoxelizationParams& params
) {

  const int totalBlocks = (params.resolutionZ + params.slicesPerBlock - 1) / params.slicesPerBlock;
  const float deltaZ = zSpan / params.resolutionZ;
  const int totalPixels = size_t(params.resolution) * size_t(params.resolution);

  // Allocate texture to store slice block (N slices per block)
  glBindTexture(GL_TEXTURE_2D_ARRAY, sliceTex);
  glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, params.resolution, params.resolution, params.slicesPerBlock + 1);

  // Allocate buffers
  GLuint transitionBuffer, countBuffer, overflowBuffer;

  glGenBuffers(1, &transitionBuffer);
  glGenBuffers(1, &countBuffer);
  glGenBuffers(1, &overflowBuffer);

  // Transition buffer: store Z transitions for each pixel --------------------
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionBuffer);
  //glBufferData(GL_SHADER_STORAGE_BUFFER, totalPixels * resolutionZ * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); //@@@ Too big for resolutions over 512x512x512
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalPixels * params.maxTransitionsPerZColumn * sizeof(GLuint),  nullptr, GL_DYNAMIC_COPY);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, transitionBuffer);

  // Count buffer: store count of transitions for each pixel ------------------
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, countBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalPixels * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, countBuffer);
  // Initialize count buffer to zero
  std::vector<GLuint> zeroCounts(totalPixels, 0);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalPixels * sizeof(GLuint), zeroCounts.data());

  // Overflow buffer: store overflow flags for each pixel --------------------
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, overflowBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER,
    totalPixels * sizeof(GLuint),
                nullptr,
                GL_DYNAMIC_COPY);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, overflowBuffer);
  // Initialize overflow buffer to zero
  std::vector<GLuint> zeroFlags(totalPixels, 0);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                  zeroFlags.size() * sizeof(GLuint),
                  zeroFlags.data());

  glm::mat4 projection = glm::ortho(-0.51f, 0.51f, -0.51f, 0.51f, 0.0f, zSpan);
  glm::mat4 view = glm::lookAt(glm::vec3(0, 0, zSpan / 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, params.resolution, params.resolution);
  glEnable(GL_DEPTH_TEST);

  auto startTime = std::chrono::high_resolution_clock::now();

  for (int block = 0; block < totalBlocks; ++block) {
    int zStart = block * params.slicesPerBlock;
    int slicesThisBlock = std::min(params.slicesPerBlock, params.resolutionZ - zStart);

    // Render SLICES_PER_BLOCK+1 slices, overlapping one with previous block
    for (int i = 0; i <= slicesThisBlock; ++i) {
      float z = zSpan / 2.0f - (zStart + i - 1) * deltaZ; // slice 0 is black for i = 0
      glm::vec4 clipPlane(0, 0, -1, z);

      drawShader->use();
      drawShader->setMat4("projection", projection);
      drawShader->setMat4("view", view);
      drawShader->setMat4("model", glm::mat4(1.0f));
      drawShader->setVec4("clippingPlane", clipPlane);

      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, sliceTex, 0, i);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      if (zStart + i - 1 >= 0) {
        glBindVertexArray(mesh.vao);
        glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);
      }
    }

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    computeShader->use();
    computeShader->setInt("zStart", zStart);
    computeShader->setInt("sliceCount", slicesThisBlock);
    computeShader->setInt("resolution", params.resolution);
    computeShader->setInt("resolutionZ", params.resolutionZ);

    glBindImageTexture(0, sliceTex, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
    // e.g. if resolution is 1024, this will launch 64 x 64 x slicesThisBlock workgroups
    // Then the compute shader will launch 16 threads per workgroup in xy, thus getting "resolution" threads in xy
    // 16 is a good compromise, since e.g. almost any resolution can be divided by 16
    glDispatchCompute(params.resolution / 16, params.resolution / 16, slicesThisBlock);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  }

  glFinish();

  auto endTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsedTime = endTime - startTime;
  std::cout << "Voxelization complete. Execution time: " << elapsedTime.count() << " seconds\n";

  // Read back the overflow buffer to check for overflow --------------------
  std::vector<GLuint> overflowFlags(totalPixels);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, overflowBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    overflowFlags.size() * sizeof(GLuint),
                    overflowFlags.data());

  for (int i = 0; i < totalPixels; ++i) {
      if (overflowFlags[i]) {
          std::cout << "Overflow at pixel column " << i << "\n";
      }
  }
  // --------------------------------------------------------------------------

  //@@@ Optional: read back the transition buffer for debugging ---------------
  std::vector<GLuint> hostCounts(totalPixels);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, countBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, hostCounts.size() * sizeof(GLuint), hostCounts.data());

  std::vector<GLuint> hostTransitions(totalPixels * params.maxTransitionsPerZColumn);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, hostTransitions.size() * sizeof(GLuint), hostTransitions.data());

  // Example: print Z transitions for pixel (x=300, y=290)
  int colIndex = 290 * params.resolution + 300;
  for (uint i = 0; i < hostCounts[colIndex]; ++i) {
      std::cout << "Transition Z: " << hostTransitions[colIndex * params.maxTransitionsPerZColumn + i] << "\n";
  }
  // --------------------------------------------------------------------------

  glDeleteBuffers(1, &transitionBuffer);
  glDeleteBuffers(1, &countBuffer);

}
