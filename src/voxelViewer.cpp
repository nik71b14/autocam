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

  window = glfwCreateWindow(params.resolutionXYZ.x, params.resolutionXYZ.y, "Voxel Transition Viewer", nullptr, nullptr);
  if (!window) throw std::runtime_error("Failed to create GLFW window");

  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
      throw std::runtime_error("Failed to initialize GLAD");

  // Mouse callbacks
  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(window, [](GLFWwindow* win, double xpos, double ypos) {
      static_cast<VoxelViewer*>(glfwGetWindowUserPointer(win))->onMouseMove(xpos, ypos);
  });
  glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
      static_cast<VoxelViewer*>(glfwGetWindowUserPointer(win))->onMouseButton(button, action, mods);
  });
  glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
      static_cast<VoxelViewer*>(glfwGetWindowUserPointer(win))->onScroll(xoffset, yoffset);
  });
  

  glfwSwapInterval(1);
  glViewport(0, 0, params.resolutionXYZ.x, params.resolutionXYZ.y);
}

void VoxelViewer::onMouseMove(double xpos, double ypos) {
  glm::vec2 mousePos(xpos, ypos);
  glm::vec2 delta = mousePos - lastMousePos;

  if (leftMousePressed) {
    yaw += delta.x * 0.3f;
    pitch -= delta.y * 0.3f;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
  } else if (rightMousePressed) {
    panOffset += delta * 0.005f;
  }

  lastMousePos = mousePos;
}

void VoxelViewer::onMouseButton(int button, int action, int mods) {
  (void)mods; // Ignore modifiers for now
  if (button == GLFW_MOUSE_BUTTON_LEFT) leftMousePressed = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_RIGHT) rightMousePressed = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_MIDDLE) middleMousePressed = (action == GLFW_PRESS);

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);
  lastMousePos = glm::vec2(xpos, ypos);
}

void VoxelViewer::onScroll(double xoffset, double yoffset) {
  (void)xoffset; // Ignore horizontal scroll for now
  distance *= (1.0f - yoffset * 0.1f);
  distance = glm::clamp(distance, 0.1f, 10.0f);
  // std::cout << "Current distance: " << distance << std::endl;
}


void VoxelViewer::setupShaderAndBuffers() {
  raymarchingShader = new Shader("shaders/raymarching.vert", "shaders/raymarching.frag");
  raymarchingShader->use();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  //glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

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

  raymarchingShader->use();
  raymarchingShader->setIVec3("resolution", 
      glm::ivec3(params.resolutionXYZ.x, params.resolutionXYZ.y, params.resolutionXYZ.z));
  raymarchingShader->setInt("maxTransitions", params.maxTransitionsPerZColumn);

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Get current window size
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Calculate aspect ratio from current window size
    float windowAspect = (float)width / (float)height;

    // Calculate object dimensions based on voxelization parameters
    float voxelScale = 1.0f / std::max(params.resolutionXYZ.x, params.resolutionXYZ.y); // or Z if 3D
    float objectWidth = params.resolutionXYZ.x * voxelScale;
    float objectHeight = params.resolutionXYZ.y * voxelScale;
    float objectAspect = objectWidth / objectHeight;

    // Fit object in view while preserving aspect ratio
    float viewWidth, viewHeight;

    if (windowAspect > objectAspect) {
      // Window is wider than object → expand horizontally
      viewHeight = objectHeight;
      viewWidth = objectHeight * windowAspect;
    } else {
        // Window is taller than object → expand vertically
        viewWidth = objectWidth;
        viewHeight = objectWidth / windowAspect;
    }

    // Set up projection matrix
    glm::mat4 proj;
    if (this->ortho) {
        // Use symmetric frustum that matches window aspect ratio
        proj = glm::ortho(
          -viewWidth / 2.0f, viewWidth / 2.0f,
          -viewHeight / 2.0f, viewHeight / 2.0f,
          0.1f, 100.0f
        );
    } else {
        proj = glm::perspective(glm::radians(45.0f), windowAspect, 0.1f, 100.0f);
    }
    
    // Camera setup
    // glm::vec3 cameraPos(0, 0, 2.0);
    // glm::vec3 pointToLookAt(0, 0, 0);
    // glm::vec3 upVector(0, 1, 0);
    // glm::mat4 view = glm::lookAt(cameraPos, pointToLookAt, upVector);

    // Calculate camera position based on pitch, yaw, distance and target (mouse interaction)
    glm::vec3 dir;
    dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

    glm::vec3 cameraPos = target + glm::vec3(panOffset, 0.0f) + (-dir * distance);
    glm::mat4 view = glm::lookAt(cameraPos, target + glm::vec3(panOffset, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Calculate inverse view-projection matrix
    glm::mat4 invViewProj = glm::inverse(proj * view);

    raymarchingShader->setMat4("invViewProj", invViewProj);
    raymarchingShader->setVec3("cameraPos", cameraPos);
    raymarchingShader->setIVec2("screenResolution", glm::ivec2(width, height));

    renderFullScreenQuad();
    glfwSwapBuffers(window);
  }

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
