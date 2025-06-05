#include "voxelizer.hpp"

#include <glad/glad.h>
#include "shader.hpp"
#include "GLUtils.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <numeric>
#include <random>

#include "prefixSum.hpp"

#define MIN_RESOLUTION_XYZ 256 // Minimum resolution for each axis, used for very small objects
#define DEBUG_OUTPUT // Enable debug output for detailed information
//#define DEBUG_GPU // Enable OpenGL debug output
#define GPU_LIMITS // Enable GPU limits output
#define WORKGROUP_SIZE 1'024

// Default constructor
Voxelizer::Voxelizer() {}

Voxelizer::Voxelizer(const VoxelizationParams& params)
    : params(params) {}

Voxelizer::Voxelizer(const Mesh& mesh, VoxelizationParams& params): mesh(mesh), params(params) {
    vertices = mesh.vertices;
    indices = mesh.indices;

    glm::ivec3 res = this->calculateResolutionPx(vertices); // Calculate resolution based on mesh vertices
    params.resolutionXYZ = res; // Modify params passed in constructor by reference
    this->params.resolutionXYZ = res; // Modify class member params.resolutionXYZ
    normalizeMesh(); // Normalize the mesh vertices
}

void Voxelizer::setMesh(const Mesh& newMesh) {
    mesh = newMesh;
    vertices = newMesh.vertices;
    indices = newMesh.indices;

    glm::ivec3 res = this->calculateResolutionPx(vertices); // Calculate resolution based on mesh vertices
    params.resolutionXYZ = res; // Modify params passed in constructor by reference
    this->params.resolutionXYZ = res; // Modify class member params.resolutionXYZ
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

  #ifdef DEBUG_OUTPUT
  std::cout << "Calculated resolution: "
            << resX << " (X) x "
            << resY << " (Y) x "
            << resZ << " (Z) [px]" << std::endl;
  #endif

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
  setupGL(&window, params.resolutionXYZ.x, params.resolutionXYZ.y, "STL Viewer", !params.preview);
  if (!window) throw std::runtime_error("Failed to create GLFW window");

  #ifdef DEBUG_GPU
  queryGPULimits();
  #endif

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

  #ifdef GPU_LIMITS
  // ##########################################################################
  // Check not to exceed CPU limits for texture
  GLint maxTexSize, maxTexLayers;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);             // max width/height for 2D/3D
  // glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maxTexSize);       // for GL_TEXTURE_3D
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxTexLayers);   // for GL_TEXTURE_2D_ARRAY

  if (params.resolutionXYZ.x > maxTexSize ||
    params.resolutionXYZ.y > maxTexSize ||
    params.slicesPerBlock + 1 > maxTexLayers) {
    std::cerr << "ERROR: Texture dimensions exceed GPU limits!" << std::endl;
  }
  // ##########################################################################
  #endif

  // Create 2D array texture to hold Z slices @@@MOVE TO createFramebufferZ
  glGenTextures(1, &sliceTex);
  glBindTexture(GL_TEXTURE_2D_ARRAY, sliceTex);
  glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, params.resolutionXYZ.x, params.resolutionXYZ.y, params.slicesPerBlock + 1);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Create framebuffer
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  // Attach the 0th layer of the 2D array texture
  glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, sliceTex, 0, 0); //&&&

  glDrawBuffer(GL_COLOR_ATTACHMENT0);

  const int totalBlocks = (params.resolutionXYZ.z + params.slicesPerBlock - 1) / params.slicesPerBlock;
  const float deltaZ = zSpan / params.resolutionXYZ.z;
  size_t totalPixels = size_t(params.resolutionXYZ.x) * size_t(params.resolutionXYZ.y);

  // Allocate buffers
  GLuint transitionBuffer, countBuffer, overflowBuffer;

  glGenBuffers(1, &transitionBuffer);
  glGenBuffers(1, &countBuffer);
  glGenBuffers(1, &overflowBuffer);

  // Transition buffer: store Z transitions for each pixel --------------------
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionBuffer);
  //glBufferData(GL_SHADER_STORAGE_BUFFER, totalPixels * resolutionXYZ.z * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); //@@@ Too big for resolutions over 512x512x512
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

  //glm::mat4 projection = glm::ortho(-0.51f, 0.51f, -0.51f, 0.51f, 0.0f, zSpan);
  glm::mat4 projection = glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, 0.0f, 100*zSpan); // Changed here from 1*zSpan to 10*zSpan
  glm::mat4 view = glm::lookAt(glm::vec3(0, 0, zSpan / 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, params.resolutionXYZ.x, params.resolutionXYZ.y);
  glEnable(GL_DEPTH_TEST);

  // Attach a depth renderbuffer to the framebuffer, which is needed for depth testing
  GLuint depthRbo;
  glGenRenderbuffers(1, &depthRbo);
  glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, params.resolutionXYZ.x, params.resolutionXYZ.y);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Framebuffer not complete! Error code: 0x" << std::hex << status << std::endl;
  }

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
    int slicesThisBlock = std::min(params.slicesPerBlock, params.resolutionXYZ.z - zStart);

    
    // Render SLICES_PER_BLOCK + 1 slices, overlapping one with previous block
    for (int i = 0; i <= slicesThisBlock; ++i) {
      float z = zSpan / 2.0f - (zStart + i - 1) * deltaZ; // slice 0 is black for i = 0
      glm::vec4 clipPlane(0, 0, -1, z);

      glBindFramebuffer(GL_FRAMEBUFFER, fbo); // Rebind the custom FBO
      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, sliceTex, 0, i);

      drawShader->use();
      drawShader->setMat4("projection", projection);
      drawShader->setMat4("view", view);
      drawShader->setMat4("model", glm::mat4(1.0f));
      drawShader->setVec4("clippingPlane", clipPlane);

      //glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, sliceTex, 0, i); // bind texture layer for this slice
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      if (zStart + i - 1 >= 0) {
        glBindVertexArray(meshBuffers.vao);
        glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);
      }

      // Visualize the slice
      if (params.preview) {
        //glEnable(GL_DEPTH_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, 0); // Bind default framebuffer
        glViewport(0, 0, params.resolutionXYZ.x, params.resolutionXYZ.y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawShader->use();
        drawShader->setMat4("projection", projection);
        drawShader->setMat4("view", view);
        drawShader->setMat4("model", glm::mat4(1.0f));
        drawShader->setVec4("clippingPlane", clipPlane);

        glBindVertexArray(meshBuffers.vao);
        glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);

        // Introduce a delay to slow down the slicing process
        //std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }

    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);

    computeShader->use();
    computeShader->setInt("zStart", zStart);
    computeShader->setInt("sliceCount", slicesThisBlock);
    computeShader->setInt("resolutionX", params.resolutionXYZ.x);
    computeShader->setInt("resolutionY", params.resolutionXYZ.y);
    computeShader->setInt("resolutionZ", params.resolutionXYZ.z);

    glBindImageTexture(0, sliceTex, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA8);
    // e.g. if resolution is 1024, this will launch 64 x 64 x slicesThisBlock workgroups
    // Then the compute shader will launch 16 threads per workgroup in xy, thus getting "resolution" threads in xy
    // 16 is a good compromise, since e.g. almost any resolution can be divided by 16
    //glDispatchCompute(params.resolutionXYZ.x / 16, params.resolutionXYZ.y / 16, slicesThisBlock);

    // This accomodates the case where resolutionXYZ.x and resolutionXYZ.y are not equal and not divisible by 16
    glDispatchCompute(
      (params.resolutionXYZ.x + 15) / 16,
      (params.resolutionXYZ.y + 15) / 16,
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
  
  // Graphical output of the voxelization results
  #ifdef DEBUG_GPU
  std::vector<GLuint> counts(totalPixels);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, countBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, counts.size() * sizeof(GLuint), counts.data());

  // Read back the transition buffer, i.e. Z transitions for each pixel column
  std::vector<GLuint> hostTransitions(totalPixels * params.maxTransitionsPerZColumn);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionBuffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, hostTransitions.size() * sizeof(GLuint), hostTransitions.data());

  // Plot a transition map as seen from above, where each pixel column is represented by a character # if it has at least one transition, or . if it has no transitions.
  const int maxCols = 50;
  const int maxRows = 50;
  int strideX = std::max(1, params.resolutionXYZ.x / maxCols);
  int strideY = std::max(1, params.resolutionXYZ.y / maxRows);

  std::cout << "XY transition map (# = has transition):\n";
  for (int row = 0; row < maxRows; ++row) {
    int y = row * strideY;
    if (y >= params.XYZ.y) break;
    for (int col = 0; col < maxCols; ++col) {
      int x = col * strideX;
      if (x >= params.resolutionXYZ.x) break;
      int idx = y * params.resolutionXYZ.x + x;
      bool hasTransition = false;
      for (uint t = 0; t < counts[idx]; ++t) {
        if (hostTransitions[idx * params.maxTransitionsPerZColumn + t] != 0) {
          hasTransition = true;
          break;
        }
      }
      std::cout << (hasTransition ? '#' : '.');
    }
    std::cout << '\n';
  }

  std::cout << "XY transition count map (0=., 1=+, 2=*, 3=#, >=4=@):\n";
  for (int row = 0; row < maxRows; ++row) {
    int y = row * strideY;
    if (y >= params.resolutionXYZ.y) break;
    for (int col = 0; col < maxCols; ++col) {
      int x = col * strideX;
      if (x >= params.resolutionXYZ.x) break;
      int idx = y * params.resolutionXYZ.x + x;
      GLuint count = counts[idx];
      char c = '.';
      if (count == 1) c = '+';
      else if (count == 2) c = '*';
      else if (count == 3) c = '#';
      else if (count >= 4) c = '@';
      std::cout << c;
    }
    std::cout << '\n';
  }

  #endif


  // ---------------------------> I HAVE A PROBLEM UP TO HERE IF THE NUMBER OF POINTS IS TOO HIGH

  // #########################################################################
  //exit(0); // Exit the program after voxelization is complete
  // #########################################################################

  // °°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°
  // EVERYTHING OK UP TO HERE
  // °°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°°


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

  // PREFIX SUM ALGORITHM
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

  /*
  // ALL THIS WORKS UP TO numBlocks = 1024, WHICH IS THE MAXIMUM NUMBER OF WORKGROUPS ALLOWED IN OPENGL *************************
  // Use a Blelloch scan with two passes
  const int WORKGROUP_SIZE = 1024; // Max local workgroup size
  const int numBlocks = (totalPixels + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE; // Number of workgroups needed (rounded up)

  if (numBlocks > 1024) {
    std::cerr << "ERROR: blockSums too large (" << numBlocks << ") for local shared memory. Max 1024 entries allowed." << std::endl;
    exit(EXIT_FAILURE);
  }

  // 1. Create prefix sum output buffer (same size as countBuffer)
  GLuint prefixSumBuffer;
  glGenBuffers(1, &prefixSumBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
  //glBufferData(GL_SHADER_STORAGE_BUFFER, totalPixels * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); // Output from pass 1 and pass 3
  glBufferData(GL_SHADER_STORAGE_BUFFER, (totalPixels + 1) * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); // Output from pass 1 and pass 3 (totalPixels + 1 to handle exclusive prefix sum) //$$$$$

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
  // ****************************************************************************************************************************
  */

  // ALGORITHM FOR ARBITRARY NUMBER OF WORKGROUPS (e.g. 2048, 4096, etc.) *******************************************************

  // Steps for multi-level prefix sum (Blelloch scan):

  // 1. Pass 1 - Local scan:
  //    - Split the input (countBuffer) into workgroups of up to 1024 threads each.
  //    - For each workgroup:
  //      • Perform a Blelloch scan in shared memory.
  //      • Store the local scan result in prefixSumBuffer.
  //      • Store the last value (total sum of that block) in blockSums.

  // 2. Pass 2 - Recursive scan on blockSums:
  //    - If blockSums has more than 1024 entries, repeat the same strategy recursively:
  //      • Launch additional workgroups as needed.
  //      • Write results to a new buffer (e.g., blockOffsetsLevel1, etc.).
  //      • Continue recursively until the number of entries is ≤ 1024.
  //    - Do not rely on a single workgroup; dispatch multiple if needed.

  // 3. Pass 3 - Add offsets:
  //    - Add block offsets back to each block in prefixSumBuffer.

  // The recursive, multi-level prefix sum algorithm will be managed CPU side for improved flexibility and performance.

  // Level 0: scan 1024 threads per workgroup → ~1000 workgroups → 1000 blockSums
  // Level 1: scan the 1000 blockSums → ~1 workgroup → top-level sum
  // Level 2: propagate offsets back down


  // The following multile-level prefix sum algorithm is designed to handle an arbitrary number of workgroups, allowing for more than 1024 workgroups if needed.
  // It has been tested with up to 100,000,000,000 elements
  
  
  // 1. Generate dummy input data
  //const int TOTAL_ELEMENTS = 100'000'000; //@@@
  //std::vector<GLuint> _counts(TOTAL_ELEMENTS, 1); //@@@ Create and fill with 1s for testing

  // 1. Generate dummy input data
  // Fill with random integer values between 0 and 10 for testing
  // std::vector<GLuint> _counts(TOTAL_ELEMENTS);
  // std::random_device rd;
  // std::mt19937 gen(rd());
  // std::uniform_int_distribution<GLuint> dist(0, 10);
  // for (auto& val : _counts) {
  //     val = dist(gen);
  // }

  // 2. Create OpenGL buffers
  // GLuint _countBuffer; //@@@
  GLuint prefixSumBuffer, blockSumsBuffer, blockOffsetsBuffer, errorFlagBuffer;

  // glGenBuffers(1, &_countBuffer); //@@@
  // glBindBuffer(GL_SHADER_STORAGE_BUFFER, _countBuffer); //@@@
  // glBufferData(GL_SHADER_STORAGE_BUFFER, _counts.size() * sizeof(GLuint), _counts.data(), GL_STATIC_DRAW); //@@@

  glGenBuffers(1, &prefixSumBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
  // glBufferData(GL_SHADER_STORAGE_BUFFER, (_counts.size() + 1) * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); //@@@ Output buffer has TOTAL_ELEMENTS + 1 entries
  glBufferData(GL_SHADER_STORAGE_BUFFER, (totalPixels + 1) * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); // Output buffer has TOTAL_ELEMENTS + 1 entries


  glGenBuffers(1, &blockSumsBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockSumsBuffer);
  // glBufferData(GL_SHADER_STORAGE_BUFFER, div_ceil(TOTAL_ELEMENTS, WORKGROUP_SIZE) * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); //@@@
  glBufferData(GL_SHADER_STORAGE_BUFFER, div_ceil(totalPixels, WORKGROUP_SIZE) * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);


  glGenBuffers(1, &blockOffsetsBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, blockOffsetsBuffer);
  // glBufferData(GL_SHADER_STORAGE_BUFFER, div_ceil(TOTAL_ELEMENTS, WORKGROUP_SIZE) * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY); //@@@
  glBufferData(GL_SHADER_STORAGE_BUFFER, div_ceil(totalPixels, WORKGROUP_SIZE) * sizeof(GLuint), nullptr, GL_DYNAMIC_COPY);


  glGenBuffers(1, &errorFlagBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, errorFlagBuffer);
  GLuint zero = 0;
  glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint), &zero, GL_DYNAMIC_COPY);

  // 3. Load or compile shaders
  Shader* prefixPass1 = new Shader("shaders/prefix_pass1.comp");
  Shader* prefixPass2 = new Shader("shaders/prefix_pass2.comp");
  Shader* prefixPass3 = new Shader("shaders/prefix_pass3.comp");

  // 4. Run prefix sum
  prefixSumMultiLevel1B(
      countBuffer, //@@@ _countBuffer,
      prefixSumBuffer,
      blockSumsBuffer,
      blockOffsetsBuffer,
      errorFlagBuffer,
      prefixPass1,
      prefixPass2,
      prefixPass3,
      totalPixels, //@@@ TOTAL_ELEMENTS,
      WORKGROUP_SIZE
  );

  // 6. Read back results for verification
  // std::vector<GLuint> prefixResults(TOTAL_ELEMENTS + 1); //@@@
  std::vector<GLuint> prefixResults(totalPixels + 1);
  printBufferGraph(prefixSumBuffer, prefixResults.size(), 10, '*');


  // // 7. Cleanup
  // glDeleteBuffers(1, &countBuffer); //@@@ glDeleteBuffers(1, &_countBuffer);
  // glDeleteBuffers(1, &prefixSumBuffer);
  // glDeleteBuffers(1, &blockSumsBuffer);
  // glDeleteBuffers(1, &blockOffsetsBuffer);
  // glDeleteBuffers(1, &errorFlagBuffer);
  // delete prefixPass1;
  // delete prefixPass2;
  // delete prefixPass3;

  // ****************************************************************************************************************************


  // #########################################################################
  //exit(0); // Exit the program after voxelization is complete
  // #########################################################################

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
  // Delete OpenGL buffers and resources if they were created
  if (prefixSumBuffer) glDeleteBuffers(1, &prefixSumBuffer);
  if (errorFlagBuffer) glDeleteBuffers(1, &errorFlagBuffer);
  if (transitionBuffer) glDeleteBuffers(1, &transitionBuffer);
  if (countBuffer) glDeleteBuffers(1, &countBuffer);
  if (overflowBuffer) glDeleteBuffers(1, &overflowBuffer);
  if (blockSumsBuffer) glDeleteBuffers(1, &blockSumsBuffer);
  if (blockOffsetsBuffer) glDeleteBuffers(1, &blockOffsetsBuffer);
  if (compressedBuffer) glDeleteBuffers(1, &compressedBuffer);
  if (meshBuffers.vbo) glDeleteBuffers(1, &meshBuffers.vbo);
  if (meshBuffers.ebo) glDeleteBuffers(1, &meshBuffers.ebo);
  if (meshBuffers.vao) glDeleteVertexArrays(1, &meshBuffers.vao);
  if (sliceTex) glDeleteTextures(1, &sliceTex);
  if (fbo) glDeleteFramebuffers(1, &fbo);
  if (depthRbo) glDeleteRenderbuffers(1, &depthRbo);

  meshBuffers = {}; // reset values

  // Delete shader objects (the Shader destructor should handle glDeleteProgram)
  delete drawShader;
  delete computeShader;
  delete compressTransitionsShader;
  delete prefixPass1;
  delete prefixPass2;
  delete prefixPass3;

  if (window) glfwDestroyWindow(window);
  glfwTerminate();

  return {compressedData, prefixSumData};
}
