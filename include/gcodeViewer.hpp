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

    // Mouse management for camera control
    glm::vec3 cameraTarget = glm::vec3(0.0f);     // Point camera is looking at
    float cameraDistance = 50.0f;                 // Distance from camera to target (for zoom)
    float pitch = 0.0f;                           // Up/down angle
    float yaw = 0.0f;                             // Left/right angle

    glm::vec2 lastMousePos;
    bool leftButtonDown = false;
    bool rightButtonDown = false;


    // Mouse callback for camera control
    void onMouseButton(int button, int action, double xpos, double ypos);
    void onMouseMove(double xpos, double ypos);
    void onScroll(double yoffset);

    glm::vec3 getCameraDirection();
    glm::mat4 getViewMatrix();


};