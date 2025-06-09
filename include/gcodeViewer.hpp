#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "gcode.hpp"
#include "shader.hpp"

struct GLFWwindow; // Forward declaration for GLFW window to avoid prbles with glad/glad.h

class GcodeViewer {
public:
    GcodeViewer(const std::vector<GcodePoint>& toolpath);
    ~GcodeViewer();

    void setToolPosition(const glm::vec3& pos);
    bool shouldClose() const;
    void pollEvents();
    void drawFrame();

private:
    void init();
    void setupBuffers();
    void drawToolpath();
    void drawToolhead();

    Shader* shader = nullptr;
    GLFWwindow* window = nullptr;
    glm::vec3 toolPosition;

    std::vector<GcodePoint> path;
    unsigned int pathVAO = 0, pathVBO = 0;
    unsigned int sphereVAO = 0, sphereVBO = 0;
    size_t pathVertexCount = 0;
    size_t sphereVertexCount = 0;

    glm::mat4 projection, view;

    void updateCamera();
    void createShaders();
};