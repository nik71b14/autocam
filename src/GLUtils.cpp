#include "GLUtils.hpp"

#include <algorithm>
#include <glm/glm.hpp>
#include <iostream>
#include <limits>
#include <vector>
// #include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <execution>

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

  GLint maxTextureSize;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);

  GLint max3DSize;
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DSize);

  GLint maxArrayLayers;
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayLayers);

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
  std::cout << "|---------------------------------------|\n";
  std::cout << "| Max 2D texture size:                  |\n";
  std::cout << "|   " << maxTextureSize << "x" << maxTextureSize << "\n";
  std::cout << "|---------------------------------------|\n";
  std::cout << "| Max 3D texture size:                  |\n";
  std::cout << "|   " << max3DSize << "x" << max3DSize << "x" << max3DSize << "\n";
  std::cout << "|---------------------------------------|\n";
  std::cout << "| Max 2D array texture layers:          |\n";
  std::cout << "|   " << maxArrayLayers << "\n";
  std::cout << "=========================================\n";
}

int logSSBOSize(GLuint ssbo) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  GLint sizeInBytes = 0;
  glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &sizeInBytes);
  // std::cout << "Size of " << name << ": " << sizeInBytes / (1024.0 * 1024.0) << " MB\n";
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
    return {glm::vec3(0.0f), glm::vec3(0.0f)};
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

bool checkSentinelAt(GLuint ssbo, size_t sentinelIndex, GLuint expectedMarker) {
  GLuint sentinelValue = 0;

  // Memory barrier to ensure SSBO writes are complete
  glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

  // Bind the buffer
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

  // Check buffer size in bytes and convert to GLuint count
  GLint bufferSizeBytes = 0;
  glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &bufferSizeBytes);
  size_t bufferSizeUints = bufferSizeBytes / sizeof(GLuint);

  // Validate index
  if (sentinelIndex >= bufferSizeUints) {
    std::cerr << "ERROR: Sentinel index " << sentinelIndex << " exceeds buffer size (" << bufferSizeUints << ")\n";
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return false;
  }

  // Read sentinel
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, sentinelIndex * sizeof(GLuint), sizeof(GLuint), &sentinelValue);

  // Unbind
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  // Check marker
  if (sentinelValue != expectedMarker) {
    std::cerr << "ERROR: Sentinel mismatch (read: 0x" << std::hex << sentinelValue << ", expected: 0x" << expectedMarker << ")\n";
    return false;
  }

  std::cout << "Sentinel check passed at index " << std::dec << sentinelIndex << ": 0x" << std::hex << sentinelValue << "\n";
  return true;
}

GLuint sumUIntBuffer(GLuint ssbo, size_t numElements) {
  // 1. Memory barrier to ensure SSBO writes are complete
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

  // 2. Error checking wrapper
  auto checkError = [](const char* msg) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
      std::cerr << "OpenGL error (" << std::hex << err << ") at: " << msg << "\n";
      return false;
    }
    return true;
  };

  // 3. Bind buffer safely
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  if (!checkError("Buffer binding")) return 0;

  // 4. Verify buffer size
  GLint bufferSize;
  glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &bufferSize);
  if (static_cast<size_t>(bufferSize) < numElements * sizeof(GLuint)) {
    std::cerr << "Buffer too small! Required: " << numElements * sizeof(GLuint) << ", Actual: " << bufferSize << "\n";
    return 0;
  }

  // 5. Map buffer instead of GetBufferSubData for large buffers
  GLuint* data = static_cast<GLuint*>(glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, numElements * sizeof(GLuint), GL_MAP_READ_BIT));
  if (!data || !checkError("Buffer mapping")) {
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    return 0;
  }

  // 6. Parallel sum for large datasets
  GLuint total = 0;

  // Using OpenMP for parallel reduction (requires OpenMP, i.e. "-fopenmp" flag in GCC compiler, insert it in tasks.json)
  // #pragma omp parallel for reduction(+:total) ----------------------------
  // for(size_t i = 0; i < numElements; ++i) {
  //     total += data[i];
  // }
  // C++17 alternative without OpenMP ---------------------------------------
  std::vector<GLuint> tmp(data, data + numElements);
  total = std::reduce(std::execution::par_unseq, tmp.begin(), tmp.end());

  // 7. Cleanup
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

  return total;
}

void renderPreview(GLuint vao, GLsizei triangleCount, GLFWwindow* window, int viewportWidth, int viewportHeight) {
  // Bind the default framebuffer (screen)
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Set viewport and clear buffers
  glViewport(0, 0, viewportWidth, viewportHeight);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Bind VAO and draw
  glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);

  // Swap the window buffers
  glfwSwapBuffers(window);

  // Optionally poll events if needed elsewhere
  // glfwPollEvents();
}

// Utility function to check for GL errors and optionally cleanup
bool checkGLError(const char* errorMessage, GLuint bufferToDelete) {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    std::cerr << errorMessage << " Error: 0x" << std::hex << err << std::dec << std::endl;
    if (bufferToDelete != 0) {
      glDeleteBuffers(1, &bufferToDelete);
    }
    return false;  // indicates error found
  }
  return true;  // no error
}

MeshBuffers uploadMesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
  MeshBuffers buffers;

  glGenVertexArrays(1, &buffers.vao);
  glGenBuffers(1, &buffers.vbo);
  glGenBuffers(1, &buffers.ebo);

  glBindVertexArray(buffers.vao);

  glBindBuffer(GL_ARRAY_BUFFER, buffers.vbo);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);  // opzionale, per "slegare" il VAO

  return buffers;
}

void setupGLContext(GLFWwindow** window, int width, int height, const std::string& title, bool hideWindow) {
  if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  if (hideWindow) {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  }

  *window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  if (!*window) throw std::runtime_error("Failed to create GLFW window");

  glfwMakeContextCurrent(*window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) throw std::runtime_error("Failed to initialize GLAD");
}

void destroyGLContext(GLFWwindow* window) {
  if (window) {
    glfwDestroyWindow(window);
    glfwTerminate();
  }
}

void createFramebuffer(GLuint& fbo, GLuint& colorTex, GLuint& depthRbo, int resolution) {
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  glGenTextures(1, &colorTex);
  glBindTexture(GL_TEXTURE_2D, colorTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, resolution, resolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

  glGenRenderbuffers(1, &depthRbo);
  glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, resolution, resolution);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    throw std::runtime_error("Framebuffer not complete.");
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);  // Optional: unbind after creation
}

std::size_t getAvailableVRAM(std::size_t maxTestSize) {
  GLuint tex = 0;
  std::size_t maxSuccessfulBytes = 0;

  // Start from 512x512 and double the size up to maxTestSize
  for (std::size_t dim = 512; dim <= maxTestSize; dim *= 2) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // Each texel in GL_R32UI is 4 bytes (32-bit unsigned integer)
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, dim, dim);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
      // Allocation failed; return last successful estimate
      // printf("Allocation failed at %zux%zu (GL error: 0x%x)\n", dim, dim, err);
      glDeleteTextures(1, &tex);
      break;
    }

    std::size_t bytes = dim * dim * sizeof(GLuint);
    // printf("Success: %zux%zu = %.2f MB\n", dim, dim, bytes / (1024.0 * 1024.0));
    maxSuccessfulBytes = bytes;

    glDeleteTextures(1, &tex);
  }

  return maxSuccessfulBytes;
}