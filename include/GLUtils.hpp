#pragma once

#include <glad/glad.h>
#include <iostream>
#include <glm/glm.hpp>
#include <vector>
#include <GLFW/glfw3.h>

void queryGPULimits();

// Logs the size of an SSBO in megabytes
int logSSBOSize(GLuint ssbo);

// Computes the axis-aligned bounding box of a list of 3D vertices.
// Input is a flat float array (x0, y0, z0, x1, y1, z1, ...).
// Returns a pair: {min, max} coordinates.
std::pair<glm::vec3, glm::vec3> computeBoundingBox(const std::vector<float>& vertices);
std::pair<glm::vec3, glm::vec3> computeBoundingBoxVec3(const std::vector<glm::vec3>& vertices);

bool checkSentinelAt(GLuint ssbo, size_t sentinelIndex, GLuint expectedMarker);

GLuint sumUIntBuffer(GLuint ssbo, size_t numElements);

void renderPreview(GLuint vao, GLsizei triangleCount, GLFWwindow* window, int viewportWidth, int viewportHeight);

bool checkGLError(const char* errorMessage, GLuint bufferToDelete = 0);

struct MeshBuffers {
  GLuint vao = 0;
  GLuint vbo = 0;
  GLuint ebo = 0;
};

// Crea VAO, VBO, EBO e carica i dati dei vertici e indici.
// Restituisce gli handle in un struct MeshBuffers.
MeshBuffers uploadMesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices);

// Libera le risorse GPU associate ai buffer passati.
void deleteMeshBuffers(MeshBuffers& buffers);

void setupGL(GLFWwindow** window, int width, int height, const std::string& title = "OpenGL Window", bool hideWindow=false);

void createFramebuffer(GLuint& fbo, GLuint& colorTex, GLuint& depthRbo, int resolution);

void destroyFramebuffer(GLuint& fbo, GLuint& colorTex, GLuint& depthRbo);
