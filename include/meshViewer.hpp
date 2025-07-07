#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <vector>

#include "shader.hpp"

class MeshViewer {
 public:
  MeshViewer(GLFWwindow* window, int windowWidth, int windowHeight, const std::vector<float>& vertices, const std::vector<int>& triangles);

  ~MeshViewer();

  void drawFrame();
  void setMeshColor(const glm::vec4& color);
  void setProjectionMode(bool usePerspective);

 private:
  GLFWwindow* window;
  int width, height;

  GLuint VAO = 0, VBO = 0, EBO = 0;
  Shader* shader = nullptr;
  Shader* shader_flat = nullptr;
  Shader* shader_raymarching = nullptr;

  std::vector<float> vertices;
  std::vector<int> triangles;

  glm::vec4 meshColor = glm::vec4(0.8f, 0.8f, 0.9f, 1.0f);
  bool useOrtho = false;

  glm::vec3 cameraPos = glm::vec3(0, 0, 3);
  glm::vec3 cameraFront = glm::vec3(0, 0, -1);
  glm::vec3 cameraUp = glm::vec3(0, 1, 0);
  float pitch = 0.0f, yaw = -90.0f;
  float zoom = 45.0f;
  float lastX = 0.0f, lastY = 0.0f;
  bool firstMouse = true;
  bool leftPressed = false;

  glm::vec3 target = glm::vec3(0.0f);  // center of orbit (mesh center)
  float radius = 10.0f;                // distance from center

  glm::mat4 getViewMatrix() const;
  glm::mat4 getProjectionMatrix() const;

  void checkContext();
  void buildMeshBuffers();
  void drawMesh();
  void updateCameraVectors();
  void createShaders();
  void fitViewToMesh();
};
