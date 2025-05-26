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
  size_t totalPixels = size_t(params.resolution) * size_t(params.resolution);

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
  
  /*
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
  */
  
  /*
  // Read back the count buffer, i.e. how many transitions were found for each pixel column
  // @@@ DEBUG
  std::vector<GLuint> counts(totalPixels);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, countBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, counts.size() * sizeof(GLuint), counts.data());

  // Read back the transition buffer, i.e. Z transitions for each pixel column
  std::vector<GLuint> hostTransitions(totalPixels * params.maxTransitionsPerZColumn);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, hostTransitions.size() * sizeof(GLuint), hostTransitions.data());

  //Example: print Z transitions for pixel (x=300, y=290)
  int colIndex = 290 * params.resolution + 300;
  for (uint i = 0; i < counts[colIndex]; ++i) {
      std::cout << "Transition Z: " << hostTransitions[colIndex * params.maxTransitionsPerZColumn + i] << "\n";
  }
  // --------------------------------------------------------------------------
  */

  // COMPRESSION OF THE TRANSITIONS BUFFER ====================================
  
  // Input:
  // transitionBuffer → [RES*RES * MAX_TRANSITIONS_PER_COLUMN] elements
  // countBuffer → [RES*RES] elements, each telling how many transitions are valid for that XY column
  //
  // Output:
  // compressedTransitionsBuffer: tightly packed transitions
  // indexBuffer: prefix sum buffer that tells where to find each column's transitions in the compressed buffer
  //
  // Strategy:
  // 1. Compute prefix sum on countBuffer to determine where to write each pixel's transitions in compressedTransitionsBuffer.
  // 2. Copy valid transitions from transitionBuffer to the correct position in compressedTransitionsBuffer.
  
  //  Each column’s compressed transitions are located starting from prefixSum[i] with countBuffer[i] entries in compressedBuffer.
  
  // MIXED GPU/CPU APPROACH
  /*
  std::vector<GLuint> counts(totalPixels); // CPU side buffer to read back counts of transitions per pixel column
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, countBuffer); // Work on countBuffer
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalPixels * sizeof(GLuint), counts.data()); // Read back counts of transitions per pixel column

  // In an exclusive prefix sum, each prefix[i] is the sum of all counts[j] before i, not including counts[i].
  // Example:
  // counts = [3, 1, 4, 0, 2]
  //
  // prefix[0] = 0
  // prefix[1] = 3       // = counts[0]
  // prefix[2] = 3 + 1   // = counts[0] + counts[1]
  // prefix[3] = 3 + 1 + 4
  // prefix[4] = 3 + 1 + 4 + 0
  //
  // Result: prefix = [0, 3, 4, 8, 8], which is what i we need (prefix is the offset in the compressed buffer where each pixel's transitions start).
  std::vector<GLuint> prefix(totalPixels, 0);
  for (size_t i = 1; i < totalPixels; ++i) {
      prefix[i] = prefix[i - 1] + counts[i - 1];
  }

  // Upload to GPU
  GLuint prefixSumBuffer;
  glGenBuffers(1, &prefixSumBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer); // 1. Bind before allocating or uploading
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalPixels * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); // 2. Allocate storage
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalPixels * sizeof(GLuint), prefix.data()); // 3. Upload data

  Shader* compressTransitionsShader = new Shader("shaders/compress_transitions.comp");
  GLuint compressedBuffer;

  // Read total compressed size (last prefix sum + count)
  // I need to add to the last prefix sum the count of transitions in the last pixel column!

  std::cout << "Last prefix sum: " << prefix.back() << "\n";
  std::cout << "Last value in counts: " << counts.back() << "\n";
  GLuint totalCompressedCount = prefix.back() + counts.back();

  std::cout << "Total compressed count: " << totalCompressedCount << "\n";

  // Allocate compressed transition buffer
  glGenBuffers(1, &compressedBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, compressedBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalCompressedCount * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);

  // Run compression shader
  compressTransitionsShader->use(); // Bind `compress_transitions.comp`
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transitionBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, countBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, prefixSumBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, compressedBuffer);

  glDispatchCompute((totalPixels + 255) / 256, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  //@@@ Check size of compressed buffer
  GLint64 compressedSize = 0;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, compressedBuffer);
  glGetBufferParameteri64v(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &compressedSize);
  std::cout << "Compressed GPU buffer size: " << (compressedSize / (1024.0 * 1024.0)) << " MB" << std::endl;
  */

  // FULL GPU APPROACH

  // -----> PREFIX SUM ALGORITHM <-----

  // Use a Blelloch scan with two passes
  const int WORKGROUP_SIZE = 1024; // Max local workgroup size
  const int numBlocks = (totalPixels + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE; // Number of workgroups needed (rounded up)

  //@@@ ENABLE GL DEBUG
  /*
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity,
                            GLsizei length, const GLchar* message, const void* userParam) {
      std::cerr << "GL DEBUG: " << message << "\n";
  }, nullptr);
  */

  // 1. Create prefix sum output buffer (same size as countBuffer)
  GLuint prefixSumBuffer;
  glGenBuffers(1, &prefixSumBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalPixels * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); // Output from pass 1 and pass 3

  // 2. Create blockSums buffer (one entry per block)
  GLuint blockSumsBuffer;
  glGenBuffers(1, &blockSumsBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockSumsBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, numBlocks * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); // Output from pass 1, input to pass 2

  // 3. Create blockOffsets buffer (also one entry per block)
  GLuint blockOffsetsBuffer;
  glGenBuffers(1, &blockOffsetsBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockOffsetsBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, numBlocks * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); // Output from pass 2, input to pass 3

  // Load and build the prefix sum shaders
  Shader* prefixPass1 = new Shader("shaders/prefix_sum_pass1.comp");
  Shader* prefixPass2 = new Shader("shaders/prefix_sum_pass2.comp");
  Shader* prefixPass3 = new Shader("shaders/prefix_sum_pass3.comp");

  // Dispatch sequence
  // Pass 1: Local scan and blockSums
  prefixPass1->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, countBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, blockSumsBuffer);
  glDispatchCompute(numBlocks, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // Pass 2: Scan blockSums into blockOffsets
  prefixPass2->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, blockSumsBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, blockOffsetsBuffer);
  glDispatchCompute(1, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // Pass 3: Add blockOffsets
  prefixPass3->use();
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, blockOffsetsBuffer);
  glDispatchCompute(numBlocks, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  //@@@ DEBUG CHECKS
  /*
  // Download result from GPU
  std::vector<GLuint> prefixSumResult(totalPixels);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalPixels * sizeof(GLuint), prefixSumResult.data());

  // Print 100 values uniformly taken from prefixSumResult
  std::cout << "Graphical representation of prefixSumResult:\n";
  size_t step = totalPixels / 10;
  for (size_t i = 0; i < totalPixels; i += step) {
    int barLength = prefixSumResult[i] / 100000; // Scale down for terminal display
    std::cout << "[" << i << "] ";
    for (int j = 0; j < barLength; ++j) {
      std::cout << ".";
    }
    std::cout << " (" << prefixSumResult[i] << ")\n";
  }
  */

  // -----> COMPRESSION <-----

  // 1. Read the last value from prefixSumBuffer (from GPU).
  // 2. Read the last value from countBuffer (from GPU).
  // 3. Add them on the CPU to compute totalCompressedCount.
  // 4. Use that to allocate compressedBuffer.
  
  Shader* compressTransitionsShader = new Shader("shaders/compress_transitions.comp");

  // --- 1. Read last value from prefixSumBuffer
  GLuint lastPrefixValue = 0;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, (totalPixels - 1) * sizeof(GLuint), sizeof(GLuint), &lastPrefixValue);

  // --- 2. Read last value from countBuffer
  GLuint lastCountValue = 0;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, countBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, (totalPixels - 1) * sizeof(GLuint), sizeof(GLuint), &lastCountValue);

  // --- 3. Compute total compressed count
  GLuint totalCompressedCount = lastPrefixValue + lastCountValue;

  std::cout << "Last prefix sum (GPU): " << lastPrefixValue << "\n";
  std::cout << "Last count (GPU): " << lastCountValue << "\n";
  std::cout << "Total compressed count: " << totalCompressedCount << "\n";

  // --- 4. Allocate compressed output buffer
  GLuint compressedBuffer;
  glGenBuffers(1, &compressedBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, compressedBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalCompressedCount * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);

  // --- 5. Dispatch compression shader
  compressTransitionsShader->use(); // Assume already compiled
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, transitionBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, countBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, prefixSumBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, compressedBuffer);

  glDispatchCompute((totalPixels + 255) / 256, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // --- 6. Debug: Check buffer size
  GLint64 compressedSize = 0;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, compressedBuffer);
  glGetBufferParameteri64v(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &compressedSize);
  std::cout << "Compressed GPU buffer size: " << (compressedSize / (1024.0 * 1024.0)) << " MB\n";

  // ==========================================================================
  
  // Cleanup
  glDeleteBuffers(1, &transitionBuffer);
  glDeleteBuffers(1, &countBuffer);
  glDeleteBuffers(1, &overflowBuffer);
  glDeleteBuffers(1, &prefixSumBuffer);
  glDeleteBuffers(1, &blockSumsBuffer);
  glDeleteBuffers(1, &blockOffsetsBuffer);
  glDeleteBuffers(1, &compressedBuffer);
  glDeleteTextures(1, &sliceTex);

  delete compressTransitionsShader;
  delete prefixPass1;
  delete prefixPass2;
  delete prefixPass3;
  
}
