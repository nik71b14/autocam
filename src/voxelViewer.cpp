#include "voxelViewer.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "voxelizer.hpp"

VoxelViewer::VoxelViewer(
  const std::string& compressedFile,
  const std::string& prefixSumFile,
  VoxelizationParams params
): params(params){
  if (!loadBinaryFile(compressedFile, compressedData) ||
    !loadBinaryFile(prefixSumFile, prefixSumData)) {
    throw std::runtime_error("Failed to load one or both input files");
  }
  initGL();
  setupShaderAndBuffers();
}

VoxelViewer::VoxelViewer(
  const std::vector<unsigned int>& compressed,
  const std::vector<unsigned int>& prefixSum,
  VoxelizationParams params): params(params), compressedData(compressed), prefixSumData(prefixSum){
  initGL();
  setupShaderAndBuffers();
}

VoxelViewer::~VoxelViewer() {
  glDeleteBuffers(1, &compressedBuffer);
  glDeleteBuffers(1, &prefixSumBuffer);
  glDeleteBuffers(1, &quadVBO);
  glDeleteVertexArrays(1, &quadVAO);
  delete raymarchingShader;
  glfwDestroyWindow(window);
  glfwTerminate();
}

void VoxelViewer::initGL() {
  if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

  window = glfwCreateWindow(params.resolutionX, params.resolutionY, "Voxel Transition Viewer", nullptr, nullptr);
  if (!window) throw std::runtime_error("Failed to create GLFW window");

  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
      throw std::runtime_error("Failed to initialize GLAD");

  glfwSwapInterval(1);
  glViewport(0, 0, params.resolutionX, params.resolutionY);
}

void VoxelViewer::setupShaderAndBuffers() {
  raymarchingShader = new Shader("shaders/raymarching.vert", "shaders/raymarching.frag");
  raymarchingShader->use();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

  float quadVertices[] = {
      -1.0f, -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,
      -1.0f,  1.0f,  1.0f, -1.0f,   1.0f,  1.0f,
  };

  glGenVertexArrays(1, &quadVAO);
  glGenBuffers(1, &quadVBO);
  glBindVertexArray(quadVAO);
  glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

  glGenBuffers(1, &compressedBuffer);
  glGenBuffers(1, &prefixSumBuffer);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, compressedBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, compressedData.size() * sizeof(GLuint), compressedData.data(), GL_DYNAMIC_COPY);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, prefixSumData.size() * sizeof(GLuint), prefixSumData.data(), GL_DYNAMIC_COPY);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, compressedBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);
}

void VoxelViewer::run() {

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Get current window size
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    raymarchingShader->use();
    raymarchingShader->setIVec3("resolution", 
        glm::ivec3(params.resolutionX, params.resolutionY, params.resolutionZ));
    raymarchingShader->setInt("maxTransitions", params.maxTransitionsPerZColumn);

    // Calculate aspect ratio from current window size
    float aspect = (float)width / (float)height;
    
    // Set up projection matrix
    glm::mat4 proj;
    if (this->ortho) {
        // Use symmetric frustum that matches window aspect ratio
        float viewSize = 1.0f;  // Base size
        proj = glm::ortho(
            -viewSize * aspect / 2, viewSize * aspect / 2,  // left, right
            -viewSize / 2, viewSize / 2,                    // bottom, top
            0.1f, 100.0f
        );
    } else {
        proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    }
    
    // Camera setup
    glm::vec3 cameraPos(0, 0, 2.0);
    glm::vec3 pointToLookAt(0, 0, 0);
    glm::vec3 upVector(0, 1, 0);
    glm::mat4 view = glm::lookAt(cameraPos, pointToLookAt, upVector);
    
    glm::mat4 invViewProj = glm::inverse(proj * view);

    raymarchingShader->setMat4("invViewProj", invViewProj);
    raymarchingShader->setVec3("cameraPos", cameraPos);
    raymarchingShader->setIVec2("screenResolution", glm::ivec2(width, height));

    renderFullScreenQuad();
    glfwSwapBuffers(window);
  }

  /*
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    //glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    raymarchingShader->use();
    raymarchingShader->setIVec3("resolution", glm::ivec3(this->params.resolutionX, this->params.resolutionY, this->params.resolutionZ));
    raymarchingShader->setInt("maxTransitions", this->params.maxTransitionsPerZColumn);

    // Use either perspective or orthographic projection as needed7
    float aspect = (float)params.resolutionX / (float)params.resolutionY;
    glm::mat4 proj;
    if (this->ortho) {
      // Use symmetric frustum that matches aspect ratio
      float viewHeight = 1.0f;
      float viewWidth = viewHeight * aspect;
      proj = glm::ortho(
          -viewWidth/2, viewWidth/2,  // left, right
          -viewHeight/2, viewHeight/2, // bottom, top
          0.1f, 100.0f
      );
    } else {
        proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    }
    glm::vec3 cameraPos(0, 0, 2.0); // Position the camera at (0, 0, 2.0) - this is relevant for perspective projection, not for orthographic
    glm::vec3 pointToLookAt = glm::vec3(0, 0, 0); // Look at the origin
    glm::vec3 upVector(0, 1, 0); // Up vector for the camera
    glm::mat4 view = glm::lookAt(cameraPos, pointToLookAt, upVector);
    glm::mat4 invViewProj = glm::inverse(proj * view);

    raymarchingShader->setMat4("invViewProj", invViewProj);
    raymarchingShader->setVec3("cameraPos", glm::vec3(0,0,2));
    raymarchingShader->setIVec2("screenResolution", glm::ivec2(params.resolutionX, params.resolutionY));

    renderFullScreenQuad();
    glfwSwapBuffers(window);
  }
  */
}

void VoxelViewer::renderFullScreenQuad() {
  glBindVertexArray(quadVAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

bool VoxelViewer::loadBinaryFile(const std::string& filename, std::vector<unsigned int>& outData) {
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
      std::cerr << "Error: Cannot open file " << filename << std::endl;
      return false;
  }

  file.seekg(0, std::ios::end);
  size_t size = file.tellg();
  if (size % sizeof(unsigned int) != 0) {
      std::cerr << "Error: File size not aligned with unsigned int\n";
      return false;
  }

  file.seekg(0, std::ios::beg);
  outData.resize(size / sizeof(unsigned int));
  file.read(reinterpret_cast<char*>(outData.data()), size);
  return file.good();
}
