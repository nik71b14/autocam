#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "gcode.hpp"
#include "shader.hpp"

#define INITIAL_CAMERA_DISTANCE 50.0f
#define AXES_LENGTH 10.0f

// Sphere parameters for toolhead representation
#define SPHERE_STACKS 12
#define SPHERE_SLICES 24
#define SPHERE_RADIUS 1.0f

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
    

    Shader* shader = nullptr;
    GLFWwindow* window = nullptr;
    glm::vec3 toolPosition;
    glm::mat4 projection, view;

    void createShaders();

    // Mouse management for camera control
    bool orthographicMode = true;                               // Use orthographic projection if true
    glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, 50.0f);    // Initial camera position
    glm::vec3 cameraTarget = glm::vec3(0.0f);                   // Point camera is looking at
    float cameraDistance = INITIAL_CAMERA_DISTANCE;             // Distance from camera to target (for zoom)
    float pitch = 0.0f;                                         // Up/down angle
    float yaw = 0.0f;                                           // Left/right angle

    // Vars for orthographic projection
    glm::vec2 viewCenter = glm::vec2(0.0f);                     // Center of orthographic projection
    float viewWidth = 200.0f;                                   // Width of orthographic view

    glm::vec2 lastMousePos;
    bool leftButtonDown = false;
    bool rightButtonDown = false;


    // Mouse callback for camera control
    void onMouseButton(int button, int action, double xpos, double ypos);
    void onMouseMove(double xpos, double ypos);
    void onScroll(double yoffset);

    glm::vec3 getCameraDirection();
    glm::mat4 getViewMatrix();
    glm::mat4 getProjectionMatrix();

    // Axes
    GLuint axesVAO = 0;
    GLuint axesVBO = 0;
    bool axesInitialized = false;

    void initAxes();
    void drawAxes();

    // Toolpath
    std::vector<GcodePoint> path;
    unsigned int pathVAO = 0, pathVBO = 0;
    size_t pathVertexCount = 0;
    void drawToolpath();

    // Toolhead geometry (a sphere)
    GLuint toolheadVAO = 0;
    GLuint toolheadVBO = 0;
    int toolheadVertexCount = 0;
    bool toolheadInitialized = false;

    void initToolhead();
    void drawToolhead();
};