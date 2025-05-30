#include "voxelizer.hpp"

#include <glad/glad.h>
#include "shader.hpp"
#include "GLUtils.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <stdexcept>

#define MIN_RESOLUTION_XYZ 256 // Minimum resolution for each axis, used for very small objects
//#define DEBUG_OUTPUT // Enable debug output for detailed information
//#define DEBUG_GPU // Enable OpenGL debug output

// Default constructor
Voxelizer::Voxelizer() {}

Voxelizer::Voxelizer(const VoxelizationParams& params)
    : params(params) {}

Voxelizer::Voxelizer(const Mesh& mesh, const VoxelizationParams& params): mesh(mesh), params(params) {
    vertices = mesh.vertices;
    indices = mesh.indices;

    glm::ivec3 res = this->calculateResolutionPx(vertices); // Calculate resolution based on mesh vertices
    this->params.resolutionX = res.x;
    this->params.resolutionY = res.y;
    this->params.resolutionZ = res.z; // Set resolution based on calculated values
    normalizeMesh(); // Normalize the mesh vertices
}

void Voxelizer::setMesh(const Mesh& newMesh) {
    mesh = newMesh;
    vertices = newMesh.vertices;
    indices = newMesh.indices;

    glm::ivec3 res = this->calculateResolutionPx(vertices); // Calculate resolution based on mesh vertices
    this->params.resolutionX = res.x;
    this->params.resolutionY = res.y;
    this->params.resolutionZ = res.z; // Set resolution based on calculated values
    normalizeMesh(); // Normalize the mesh vertices
}

void Voxelizer::setParams(const VoxelizationParams& newParams) {
    params = newParams;
}

std::pair<std::vector<GLuint>, std::vector<GLuint>> Voxelizer::getResults() const {
    return { compressedData, prefixSumData };
}

void Voxelizer::clearResults() {
    compressedData.clear();
    prefixSumData.clear();
}

float Voxelizer::computeZSpan() const {
    float zMin = std::numeric_limits<float>::max();
    float zMax = std::numeric_limits<float>::lowest();

    for (size_t i = 2; i < vertices.size(); i += 3) {
        float z = vertices[i];
        zMin = std::min(zMin, z);
        zMax = std::max(zMax, z);
    }
    // return 1.01f * (zMax - zMin);
    return (zMax - zMin);

}

glm::ivec3 Voxelizer::calculateResolutionPx(const std::vector<float>& vertices) {
  if (vertices.empty() || vertices.size() % 3 != 0) {
      throw std::runtime_error("Invalid vertex data.");
  }

  glm::vec3 minCorner(std::numeric_limits<float>::max());
  glm::vec3 maxCorner(std::numeric_limits<float>::lowest());

  for (size_t i = 0; i < vertices.size(); i += 3) {
      glm::vec3 v(vertices[i], vertices[i + 1], vertices[i + 2]);
      minCorner = glm::min(minCorner, v);
      maxCorner = glm::max(maxCorner, v);
  }

  glm::vec3 modelSize = maxCorner - minCorner;

  int resX = std::max(1, static_cast<int>(std::ceil(modelSize.x / params.resolution)));
  int resY = std::max(1, static_cast<int>(std::ceil(modelSize.y / params.resolution)));
  int resZ = std::max(1, static_cast<int>(std::ceil(modelSize.z / params.resolution)));

  // Ensure resolution has a minimum size of 512x512x512, for objects that are very small
  int minRes = std::min({resX, resY, resZ});
  if (minRes < MIN_RESOLUTION_XYZ) {
    float factor = static_cast<float>(MIN_RESOLUTION_XYZ) / minRes;
    resX = static_cast<int>(std::ceil(resX * factor));
    resY = static_cast<int>(std::ceil(resY * factor));
    resZ = static_cast<int>(std::ceil(resZ * factor));
    params.resolution /= factor; // Adjust resolution to match the new resolution in terms of pixels
  }

  return glm::ivec3(resX, resY, resZ);
}

void Voxelizer::run() {
  clearResults();
  if (vertices.empty() || indices.empty()) throw std::runtime_error("Vertices or indices not set.");

  float zSpan = computeZSpan();
  auto [data, prefix] = this->voxelizerZ(vertices, indices, zSpan, params);

  compressedData = std::move(data);
  prefixSumData = std::move(prefix);
}

void Voxelizer::normalizeMesh() {
  if (vertices.empty()) {
      throw std::runtime_error("Cannot normalize: vertices are empty.");
  }

  glm::vec3 minExtents(FLT_MAX);
  glm::vec3 maxExtents(-FLT_MAX);

  // Calculate bounding box
  for (size_t i = 0; i < vertices.size(); i += 3) {
      glm::vec3 v(vertices[i], vertices[i + 1], vertices[i + 2]);
      minExtents = glm::min(minExtents, v);
      maxExtents = glm::max(maxExtents, v);
  }

  #ifdef DEBUG_OUTPUT
  std::cout << "Min Extents: (" << minExtents.x << ", " << minExtents.y << ", " << minExtents.z << ")\n";
  std::cout << "Max Extents: (" << maxExtents.x << ", " << maxExtents.y << ", " << maxExtents.z << ")\n";
  #endif

  glm::vec3 size = maxExtents - minExtents;
  this->scale = 1.0f / std::max(size.x, size.y);
  glm::vec3 center = (maxExtents + minExtents) * 0.5f;

  #ifdef DEBUG_OUTPUT
  std::cout << "Size: (" << size.x << ", " << size.y << ", " << size.z << ")\n";
  std::cout << "Scale factor: " << scale << "\n";
  std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z << ")\n";
  #endif

  // Normalize all vertices in-place
  for (size_t i = 0; i < vertices.size(); i += 3) {
      glm::vec3 v(vertices[i], vertices[i + 1], vertices[i + 2]);
      v = (v - center) * scale;
      vertices[i]     = v.x;
      vertices[i + 1] = v.y;
      vertices[i + 2] = v.z;
  }

}

std::pair<std::vector<GLuint>, std::vector<GLuint>> Voxelizer::voxelizerZ(
  const std::vector<float>& vertices,
  const std::vector<unsigned int>& indices,
  float zSpan,
  const VoxelizationParams& params
) {

  GLFWwindow* window;
  Shader* drawShader;
  Shader* computeShader;
  GLuint sliceTex, fbo;
  MeshBuffers meshBuffers;

  int triangleCount = indices.size();

  // Initialize OpenGL context and create a window
  setupGL(&window, params.resolutionX, params.resolutionY, "STL Viewer", !params.preview);
  if (!window) throw std::runtime_error("Failed to create GLFW window");

  // Enable OpenGL debug output
  #ifdef DEBUG_GPU
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback([](GLenum source, GLenum type, GLuint id, GLenum severity,
                            GLsizei length, const GLchar* message, const void* userParam) {
      (void)source; (void)type; (void)id; (void)severity; (void)length; (void)userParam; // Suppress unused parameters warning
      std::cerr << "GL DEBUG: " << message << std::endl;
  }, nullptr);
  #endif

  // Load mesh and upload to GPU
  meshBuffers = uploadMesh(vertices, indices);

  drawShader = new Shader("shaders/vertex.glsl", "shaders/fragment.glsl");
  computeShader = new Shader("shaders/transitions_xyz.comp");

  // Create 2D array texture to hold Z slices @@@MOVE TO createFramebufferZ
  glGenTextures(1, &sliceTex);
  glBindTexture(GL_TEXTURE_2D_ARRAY, sliceTex);
  glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, params.resolutionX, params.resolutionY, params.slicesPerBlock + 1);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Create framebuffer
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);

  const int totalBlocks = (params.resolutionZ + params.slicesPerBlock - 1) / params.slicesPerBlock;
  const float deltaZ = zSpan / params.resolutionZ;
  size_t totalPixels = size_t(params.resolutionX) * size_t(params.resolutionY);

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

  // glm::mat4 projection = glm::ortho(-0.51f, 0.51f, -0.51f, 0.51f, 0.0f, zSpan);
  glm::mat4 projection = glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, 0.0f, zSpan);
  glm::mat4 view = glm::lookAt(glm::vec3(0, 0, zSpan / 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, params.resolutionX, params.resolutionY);
  glEnable(GL_DEPTH_TEST);

  #ifdef DEBUG_GPU
  // Check if the shader program compiled and linked successfully
  GLint linkStatus = 0;
  glGetProgramiv(drawShader->ID, GL_LINK_STATUS, &linkStatus);
  if (linkStatus == GL_FALSE) {
      char log[1024];
      glGetProgramInfoLog(drawShader->ID, sizeof(log), nullptr, log);
      std::cerr << "Shader program link error: " << log << std::endl;
  }

  // Check if the context is current
  if (glfwGetCurrentContext() != window) {
    std::cerr << "OpenGL context is not current!" << std::endl;
  }
  #endif

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
        glBindVertexArray(meshBuffers.vao);
        glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);
      }
    }

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    computeShader->use();
    computeShader->setInt("zStart", zStart);
    computeShader->setInt("sliceCount", slicesThisBlock);
    computeShader->setInt("resolutionX", params.resolutionX);
    computeShader->setInt("resolutionY", params.resolutionY);
    computeShader->setInt("resolutionZ", params.resolutionZ);

    glBindImageTexture(0, sliceTex, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
    // e.g. if resolution is 1024, this will launch 64 x 64 x slicesThisBlock workgroups
    // Then the compute shader will launch 16 threads per workgroup in xy, thus getting "resolution" threads in xy
    // 16 is a good compromise, since e.g. almost any resolution can be divided by 16
    //glDispatchCompute(params.resolutionX / 16, params.resolutionY / 16, slicesThisBlock);

    // This accomodates the case where resolutionX and resolutionY are not equal and not divisible by 16
    glDispatchCompute(
      (params.resolutionX + 15) / 16,
      (params.resolutionY + 15) / 16,
      slicesThisBlock
    );
  
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
  }

  glFinish();

  auto endTime = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsedTime = endTime - startTime;
  std::cout << "Voxelization complete. Execution time: " << elapsedTime.count() << " seconds\n";
  
  // Read back the overflow buffer to check for overflow
  #ifdef DEBUG_GPU
  std::vector<GLuint> overflowFlags(totalPixels);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, overflowBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    overflowFlags.size() * sizeof(GLuint),
                    overflowFlags.data());

  for (size_t i = 0; i < totalPixels; ++i) {
      if (overflowFlags[i]) {
          std::cout << "Overflow at pixel column " << i << "\n";
      }
  }
  #endif
  
  // Read back the count buffer, i.e. how many transitions were found for each pixel column
  #ifdef DEBUG_GPU
  std::vector<GLuint> counts(totalPixels);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, countBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, counts.size() * sizeof(GLuint), counts.data());

  // Read back the transition buffer, i.e. Z transitions for each pixel column
  std::vector<GLuint> hostTransitions(totalPixels * params.maxTransitionsPerZColumn);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, hostTransitions.size() * sizeof(GLuint), hostTransitions.data());

  //Example: print Z transitions for pixel (x=300, y=290)
  int colIndex = 290 * params.resolutionX + 300;
  for (uint i = 0; i < counts[colIndex]; ++i) {
      std::cout << "Transition Z: " << hostTransitions[colIndex * params.maxTransitionsPerZColumn + i] << "\n";
  }
  //std::cout << "Total transitions found: " << std::accumulate(counts.begin(), counts.end(), 0) << "\n";  
  #endif

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
  
  // FULL GPU APPROACH

  // -----> PREFIX SUM ALGORITHM <-----
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

  // Use a Blelloch scan with two passes
  const int WORKGROUP_SIZE = 1024; // Max local workgroup size
  const int numBlocks = (totalPixels + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE; // Number of workgroups needed (rounded up)

  // 1. Create prefix sum output buffer (same size as countBuffer)
  GLuint prefixSumBuffer;
  glGenBuffers(1, &prefixSumBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, totalPixels * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); // Output from pass 1 and pass 3
  //glBufferData(GL_SHADER_STORAGE_BUFFER, (totalPixels + 1) * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); // Output from pass 1 and pass 3 (totalPixels + 1 to handle exclusive prefix sum)

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

  // Final checks
  #ifdef DEBUG_GPU
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
  #endif

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

  #ifdef DEBUG_GPU
  std::cout << "Last prefix sum (GPU): " << lastPrefixValue << "\n";
  std::cout << "Last count (GPU): " << lastCountValue << "\n";
  std::cout << "Total compressed count: " << totalCompressedCount << "\n";
  #endif
  
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
  std::cout << "Compressed buffer size: " << (compressedSize / (1024.0 * 1024.0)) << " MB\n";

  // Download compressedBuffer and prefixSumBuffer to CPU for further processing or visualization
  std::vector<GLuint> compressedData(totalCompressedCount);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, compressedBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalCompressedCount * sizeof(GLuint), compressedData.data());
  std::vector<GLuint> prefixSumData(totalPixels);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, totalPixels * sizeof(GLuint), prefixSumData.data());

  // Cleanup
  glDeleteBuffers(1, &transitionBuffer);
  glDeleteBuffers(1, &countBuffer);
  glDeleteBuffers(1, &overflowBuffer);
  glDeleteBuffers(1, &blockSumsBuffer);
  glDeleteBuffers(1, &blockOffsetsBuffer);
  delete drawShader;
  delete computeShader;
  delete compressTransitionsShader;
  delete prefixPass1;
  delete prefixPass2;
  delete prefixPass3;

  glDeleteBuffers(1, &meshBuffers.vbo);
  glDeleteBuffers(1, &meshBuffers.ebo);
  glDeleteVertexArrays(1, &meshBuffers.vao);
  meshBuffers = {}; // resetta i valori
  glDeleteTextures(1, &sliceTex);
  glDeleteFramebuffers(1, &fbo);
  glfwTerminate();

  return {compressedData, prefixSumData};
}
