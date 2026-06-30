#include "voxelViewer.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>

#include "coordinateSystem.hpp"
#include "voxelizer.hpp"

#define IDENTITY_MODEL glm::mat4(1.0f)  // Identity matrix for model transformations

VoxelViewer::VoxelViewer(const std::string& compressedFile, const std::string& prefixSumFile, VoxelizationParams params) : params(params) {
  if (!loadBinaryFile(compressedFile, compressedData) || !loadBinaryFile(prefixSumFile, prefixSumData)) {
    throw std::runtime_error("Failed to load one or both input files");
  }
  initGL();
  setupShaderAndBuffers();
}

VoxelViewer::VoxelViewer(const std::vector<unsigned int>& compressed, const std::vector<unsigned int>& prefixSum, VoxelizationParams params)
    : params(params), compressedData(compressed), prefixSumData(prefixSum) {
  initGL();
  setupShaderAndBuffers();

  // Auto-fit: place the camera to frame the object's real mm bounding sphere.
  // `distance` is the camera distance in millimetres (and the zoom handle for scroll).
  const CoordinateSystem cs = CoordinateSystem::fromParams(params);
  const float radiusMm = 0.5f * glm::length(cs.extentMm());
  distance = radiusMm / std::tan(glm::radians(45.0f / 2.0f)) + radiusMm;
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
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) throw std::runtime_error("Failed to initialize GLAD");

  // Mouse callbacks
  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(
      window, [](GLFWwindow* win, double xpos, double ypos) { static_cast<VoxelViewer*>(glfwGetWindowUserPointer(win))->onMouseMove(xpos, ypos); });
  glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
    static_cast<VoxelViewer*>(glfwGetWindowUserPointer(win))->onMouseButton(button, action, mods);
  });
  glfwSetScrollCallback(
      window, [](GLFWwindow* win, double xoffset, double yoffset) { static_cast<VoxelViewer*>(glfwGetWindowUserPointer(win))->onScroll(xoffset, yoffset); });

  glfwSwapInterval(1);
  glViewport(0, 0, params.resolutionXYZ.x, params.resolutionXYZ.y);
}

void VoxelViewer::onMouseMove(double xpos, double ypos) {
  glm::vec2 mousePos(xpos, ypos);
  glm::vec2 delta = mousePos - lastMousePos;

  if (leftMousePressed) {
    yaw += delta.x * 0.3f;
    // pitch -= delta.y * 0.3f; //%%%%%%%
    pitch += delta.y * 0.3f;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
  } else if (rightMousePressed) {
    panOffset += delta * 0.005f;
  }

  lastMousePos = mousePos;
}

void VoxelViewer::onMouseButton(int button, int action, int mods) {
  (void)mods;  // Ignore modifiers for now
  if (button == GLFW_MOUSE_BUTTON_LEFT) leftMousePressed = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_RIGHT) rightMousePressed = (action == GLFW_PRESS);
  if (button == GLFW_MOUSE_BUTTON_MIDDLE) middleMousePressed = (action == GLFW_PRESS);

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);
  lastMousePos = glm::vec2(xpos, ypos);
}

//%%%
// void VoxelViewer::onScroll(double xoffset, double yoffset) {
//   (void)xoffset;  // Ignore horizontal scroll for now
//   distance *= (1.0f - yoffset * 0.1f);
// }

void VoxelViewer::onScroll(double xoffset, double yoffset) {
  (void)xoffset;  // Ignore horizontal scroll for now
  distance *= (1.0f - yoffset * 0.1f);
  // `distance` is in mm now and depends on object size, so use a wide guard that
  // only prevents a non-positive (flipped) camera, not a fixed normalized range.
  distance = glm::clamp(distance, 1e-3f, 1e9f);
}

void VoxelViewer::setupShaderAndBuffers() {
  flatShader = new Shader("shaders/gcode_flat.vert", "shaders/gcode_flat.frag");
  raymarchingShader = new Shader("shaders/raymarching.vert", "shaders/raymarching.frag");

  raymarchingShader->use();

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  // glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  float quadVertices[] = {
      -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
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
  raymarchingShader->setFloat("normalizedZSpan", params.zSpan);  // Add this line
  raymarchingShader->setIVec3("resolution", glm::ivec3(params.resolutionXYZ.x, params.resolutionXYZ.y, params.resolutionXYZ.z));
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

    // --- Camera & projection in world millimetres -------------------------------
    // The object is placed and sized in mm by the model matrix (CoordinateSystem);
    // the camera auto-fits its mm bounding sphere. `distance` (mm) is the single zoom
    // handle shared by ortho and perspective, so scrolling zooms both identically.
    const CoordinateSystem cs = CoordinateSystem::fromParams(params);
    const glm::vec3 objExtentMm = cs.extentMm();
    const glm::vec3 objCenterMm = cs.centerMm;
    const float radiusMm = 0.5f * glm::length(objExtentMm);

    const float fovY = glm::radians(45.0f);
    const float nearP = std::max(0.001f * radiusMm, distance - 2.0f * radiusMm);
    const float farP = distance + 2.0f * radiusMm;

    glm::mat4 proj;
    if (this->ortho) {
      // At camera distance `distance`, a `fovY` cone spans this half-height; reusing
      // it keeps ortho and perspective framing (and the scroll zoom) consistent.
      const float halfH = distance * std::tan(fovY * 0.5f);
      const float halfW = halfH * windowAspect;
      proj = glm::ortho(-halfW, halfW, -halfH, halfH, nearP, farP);
    } else {
      proj = glm::perspective(fovY, windowAspect, nearP, farP);
    }

    // Orbit direction from yaw/pitch.
    glm::vec3 dir;
    dir.x = -sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    dir.y = sin(glm::radians(pitch));
    dir.z = cos(glm::radians(yaw)) * cos(glm::radians(pitch));

    // Pan is a fraction of the object radius, so it feels scale-independent.
    const glm::vec3 lookTarget = objCenterMm + glm::vec3(panOffset * radiusMm, 0.0f);
    const glm::vec3 cameraPos = lookTarget + dir * distance;
    const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::mat4 view = glm::lookAt(cameraPos, lookTarget, up);

    // Model matrix maps the shader's fixed local box to the mm world box (per-axis
    // scale, so non-square footprints are NOT distorted). Folding it into invViewProj
    // keeps the raymarch shader unchanged: its rays simply arrive in box-local space.
    const glm::mat4 model = cs.renderModelMatrix();

    // @@@ RENDER VOXEL
    raymarchingShader->use();
    const glm::mat4 invViewProj = glm::inverse(proj * view * model);

    raymarchingShader->setMat4("invViewProj", invViewProj);
    raymarchingShader->setVec3("cameraPos", cameraPos);
    raymarchingShader->setIVec2("screenResolution", glm::ivec2(width, height));
    raymarchingShader->setVec3("color", params.color);

    renderFullScreenQuad();
    // @@@ END RENDER VOXEL

    flatShader->use();
    flatShader->setMat4("uProj", proj);
    flatShader->setMat4("uView", view);
    flatShader->setMat4("uModel", IDENTITY_MODEL);  // axes live directly in mm world space
    drawAxes();                                     // length scaled to the object (see initAxes)

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

void VoxelViewer::drawAxes() {
  initAxes();

  flatShader->use();

  glBindVertexArray(axesVAO);

  // Red X axis
  flatShader->setVec4("uColor", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
  glDrawArrays(GL_LINES, 0, 2);

  // Green Y axis
  flatShader->setVec4("uColor", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
  glDrawArrays(GL_LINES, 2, 2);

  // Blue Z axis
  flatShader->setVec4("uColor", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
  glDrawArrays(GL_LINES, 4, 2);

  glBindVertexArray(0);
}

void VoxelViewer::initAxes() {
  if (axesInitialized) return;

  // Axis length scaled to the object so the gizmo is readable at any object size
  // (it used to be a fixed 1000 units around a ~1-unit normalized object).
  const glm::vec3 ext = CoordinateSystem::fromParams(params).extentMm();
  const float axisLen = 0.6f * std::max(ext.x, std::max(ext.y, ext.z));

  // 3 axis segments, each from the world origin to the positive direction
  std::vector<float> axesVertices = {
      0.0f, 0.0f, 0.0f, axisLen, 0.0f,    0.0f,     // X (red)
      0.0f, 0.0f, 0.0f, 0.0f,    axisLen, 0.0f,     // Y (green)
      0.0f, 0.0f, 0.0f, 0.0f,    0.0f,    axisLen   // Z (blue)
  };

  glGenVertexArrays(1, &axesVAO);
  glGenBuffers(1, &axesVBO);

  glBindVertexArray(axesVAO);
  glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
  glBufferData(GL_ARRAY_BUFFER, axesVertices.size() * sizeof(float), axesVertices.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);

  glBindVertexArray(0);

  axesInitialized = true;
}