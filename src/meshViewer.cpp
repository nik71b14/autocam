/*
PROMPT: given a mesh expressed in the form of of two arrays:
    std::vector<float> vertices;
    std::vector<int> triangles;
write a C++ class MeshViewer that takes these arrays and, using an already existing OpenGL context, and a valid GLFWwindow* window, draws the mesh on the
screen. Please include:
-> window resizing based on two parameters of the class constructor (windowWidth, windowHeight)
-> a private method to build the OpenGL buffers (VAO, VBO, EBO) for the mesh during initialization, in the constructor
-> a public method to draw the mesh, which can be called in the main loop
-> the shader to be used is defined by a variable Shader* shader = nullptr; the shader is laded using shader.hpp as follows:
  void MeshViewer::createShaders() {
    checkContext();

    shader = new Shader("shaders/gcode.vert", "shaders/gcode.frag");
    shader_flat = new Shader("shaders/gcode_flat.vert", "shaders/gcode_flat.frag");
    shader_raymarching = new Shader("shaders/raymarching.vert", "shaders/raymarching.frag");
  }
-> Create the function checkContext:
  void MeshViewer::checkContext() {
    if (!window || glfwGetCurrentContext() != window) {
      throw std::runtime_error("GLFW window is null or OpenGL context is not current for this window.");
    }
  }
-> Create a function drawFrame, something like this:
  void MeshViewer::drawFrame() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glFrontFace(GL_CCW);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(2.0f);
    glPointSize(5.0f);
    glEnable(GL_DEPTH_TEST);

    view = getViewMatrix();
    projection = getProjectionMatrix();

    // Default model matrix for static objects
    shader->use();
    shader->setMat4("uProj", projection);
    shader->setMat4("uView", view);
    shader->setMat4("uModel", IDENTITY_MODEL);  // Identity model matrix for static objects

    drawMesh();      // shader (stl source)

    glfwSwapBuffers(window);
  }

  void MeshViewer::drawTool() {
    initTool(TOOL_STL_PATH);  // Replace with actual STL path

    shader->use();
    shader->setVec4("uColor", TOOL_COLOR);

    // Pass the model matrix to the shader
    glm::mat4 model = glm::translate(IDENTITY_MODEL, toolPosition);
    shader->setMat4("uModel", model);

    glBindVertexArray(toolVAO);
    glDrawArrays(GL_TRIANGLES, 0, toolVertexCount);
    glBindVertexArray(0);
  }

-> Add also the binding to rotate, pan, zoom with the mouse
-> Add what is needed to make the object fully visible
-> Add the rendering loop
-> Add the destructor to clean up the OpenGL resources
-> Add the necessary includes and forward declarations
-> Add the necessary member variables for the OpenGL buffers (VAO, VBO, EBO) and the shader
-> Add the necessary member variables for the mesh data (vertices, triangles)
-> Add lighting setup:
  shader->use();
  shader->setVec3("lightDir", LIGHT_DIRECTION);
-> Add a method and what is needed to choose between perspective and orthographic projection
-> Add a method to set the mesh color

*/

#include "meshViewer.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>  // for std::clamp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "shader.hpp"

#define IDENTITY_MODEL glm::mat4(1.0f)
#define LIGHT_DIRECTION glm::vec3(-0.0f, -1.0f, 1.0f)

MeshViewer::MeshViewer(GLFWwindow* window, int windowWidth, int windowHeight, const std::vector<float>& vertices, const std::vector<int>& triangles,
                       const std::vector<float>& normals)
    : window(window), width(windowWidth), height(windowHeight), vertices(vertices), triangles(triangles), normals(normals) {
  checkContext();
  createShaders();
  buildMeshBuffers();

  fitViewToMesh();

  glViewport(0, 0, width, height);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int w_, int h_) {
    glViewport(0, 0, w_, h_);
    static_cast<MeshViewer*>(glfwGetWindowUserPointer(w))->width = w_;
    static_cast<MeshViewer*>(glfwGetWindowUserPointer(w))->height = h_;
  });

  glfwSetCursorPosCallback(window, [](GLFWwindow* w, double xpos, double ypos) {
    auto* self = static_cast<MeshViewer*>(glfwGetWindowUserPointer(w));
    if (!self->leftPressed) return;
    if (self->firstMouse) {
      self->lastX = xpos;
      self->lastY = ypos;
      self->firstMouse = false;
    }

    float xoffset = xpos - self->lastX;
    float yoffset = self->lastY - ypos;
    self->lastX = xpos;
    self->lastY = ypos;

    float sensitivity = 0.1f;
    self->yaw += xoffset * sensitivity;
    self->pitch -= yoffset * sensitivity;
    self->pitch = std::clamp(self->pitch, -89.0f, 89.0f);  // clamp pitch to avoid gimbal lock
    self->updateCameraVectors();
  });

  glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int) {
    auto* self = static_cast<MeshViewer*>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_LEFT) self->leftPressed = (action == GLFW_PRESS);
    if (action == GLFW_RELEASE) self->firstMouse = true;
  });

  glfwSetScrollCallback(window, [](GLFWwindow* w, double, double yoffset) {
    auto* self = static_cast<MeshViewer*>(glfwGetWindowUserPointer(w));
    self->zoom -= yoffset;
    self->zoom = std::clamp(self->zoom, 1.0f, 90.0f);
  });
}

MeshViewer::~MeshViewer() {
  glDeleteBuffers(1, &VBO);
  glDeleteBuffers(1, &EBO);
  glDeleteVertexArrays(1, &VAO);
  delete shader;
  delete shader_flat;
  delete shader_raymarching;
}

void MeshViewer::checkContext() {
  if (!window || glfwGetCurrentContext() != window) {
    throw std::runtime_error("GLFW window is null or OpenGL context is not current for this window.");
  }
}

void MeshViewer::createShaders() {
  checkContext();
  // shader = new Shader("shaders/gcode_flat.vert", "shaders/gcode_flat.frag");
  // shader = new Shader("shaders/mesh_fakenormals.vert", "shaders/mesh_fakenormals.frag");
  shader = new Shader("shaders/gcode.vert", "shaders/gcode.frag");
}

void MeshViewer::buildMeshBuffers() {
  // glGenVertexArrays(1, &VAO);
  // glGenBuffers(1, &VBO);
  // glGenBuffers(1, &EBO);

  // glBindVertexArray(VAO);

  // glBindBuffer(GL_ARRAY_BUFFER, VBO);
  // glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

  // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  // glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangles.size() * sizeof(int), triangles.data(), GL_STATIC_DRAW);

  // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
  // glEnableVertexAttribArray(0);

  // glBindVertexArray(0);

  //%%%%

  if (vertices.empty() || triangles.empty()) {
    throw std::runtime_error("MeshViewer: vertices or triangles are empty.");
  }
  if (vertices.size() % 3 != 0) {
    throw std::runtime_error("MeshViewer: vertices size must be a multiple of 3.");
  }
  if (vertices.size() != normals.size()) {
    throw std::runtime_error("MeshViewer: vertices and normals must have the same size.");
  }

  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  glBindVertexArray(VAO);

  // Interleave vertex + normal data into a single buffer
  std::vector<float> interleaved;
  for (size_t i = 0; i < vertices.size(); i += 3) {
    interleaved.push_back(vertices[i]);
    interleaved.push_back(vertices[i + 1]);
    interleaved.push_back(vertices[i + 2]);

    interleaved.push_back(normals[i]);
    interleaved.push_back(normals[i + 1]);
    interleaved.push_back(normals[i + 2]);
  }

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, interleaved.size() * sizeof(float), interleaved.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangles.size() * sizeof(int), triangles.data(), GL_STATIC_DRAW);

  // Vertex positions at location 0
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // Normals at location 1
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  //%%%%
}

glm::mat4 MeshViewer::getViewMatrix() const { return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp); }

glm::mat4 MeshViewer::getProjectionMatrix() const {
  float aspect = static_cast<float>(width) / static_cast<float>(height);
  if (useOrtho)
    return glm::ortho(-10.f * aspect, 10.f * aspect, -10.f, 10.f, -100.f, 100.f);
  else
    return glm::perspective(glm::radians(zoom), aspect, 0.1f, 1000.0f);
}

void MeshViewer::updateCameraVectors() {
  float radYaw = glm::radians(yaw);
  float radPitch = glm::radians(pitch);

  float x = radius * cos(radPitch) * cos(radYaw);
  float y = radius * sin(radPitch);
  float z = radius * cos(radPitch) * sin(radYaw);

  cameraPos = target + glm::vec3(x, y, z);
  cameraFront = glm::normalize(target - cameraPos);  // always look at target
  cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);            // you can improve this with cross product if needed
}
// void MeshViewer::updateCameraVectors() {
//   glm::vec3 front;
//   front.x = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
//   front.y = sin(glm::radians(pitch));
//   front.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
//   cameraFront = glm::normalize(front);
// }

void MeshViewer::drawMesh() {
  shader->use();
  shader->setMat4("uProj", getProjectionMatrix());
  shader->setMat4("uView", getViewMatrix());
  shader->setMat4("uModel", IDENTITY_MODEL);
  shader->setVec4("uColor", meshColor);
  shader->setVec3("lightDir", LIGHT_DIRECTION);

  glBindVertexArray(VAO);
  glDrawElements(GL_TRIANGLES, triangles.size(), GL_UNSIGNED_INT, 0);
  glBindVertexArray(0);
}

void MeshViewer::drawFrame() {
  checkContext();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glEnable(GL_DEPTH_TEST);

  drawMesh();
  glfwSwapBuffers(window);
}

void MeshViewer::setMeshColor(const glm::vec4& color) { meshColor = color; }

void MeshViewer::setProjectionMode(bool perspective) { useOrtho = perspective; }

void MeshViewer::fitViewToMesh() {
  glm::vec3 minBounds(FLT_MAX), maxBounds(-FLT_MAX);
  for (size_t i = 0; i < vertices.size(); i += 3) {
    glm::vec3 v(vertices[i], vertices[i + 1], vertices[i + 2]);
    minBounds = glm::min(minBounds, v);
    maxBounds = glm::max(maxBounds, v);
  }

  target = 0.5f * (minBounds + maxBounds);
  // radius = glm::length(maxBounds - minBounds) * 0.6f;
  radius = glm::length(maxBounds - minBounds) * 2.0f;

  yaw = -90.0f;
  pitch = 0.0f;

  updateCameraVectors();
}

// void MeshViewer::fitViewToMesh() {
//   glm::vec3 minBounds(FLT_MAX), maxBounds(-FLT_MAX);
//   for (size_t i = 0; i < vertices.size(); i += 3) {
//     glm::vec3 v(vertices[i], vertices[i + 1], vertices[i + 2]);
//     minBounds = glm::min(minBounds, v);
//     maxBounds = glm::max(maxBounds, v);
//   }
//   glm::vec3 center = 0.5f * (minBounds + maxBounds);
//   float radius = glm::length(maxBounds - minBounds) * 0.5f;

//   cameraPos = center + glm::vec3(0, 0, radius * 2.0f);
//   cameraFront = glm::normalize(center - cameraPos);
//   cameraUp = glm::vec3(0, 1, 0);
// }
