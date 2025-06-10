#include "gcodeViewer.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include "shader.hpp"

GcodeViewer::GcodeViewer(const std::vector<GcodePoint>& toolpath)
    : toolPosition(0.0f), path(toolpath)  {
    init();
    setupBuffers();
}

GcodeViewer::~GcodeViewer() {
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

void GcodeViewer::init() {
    if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(800, 600, "G-code Viewer", nullptr, nullptr);
    if (!window) throw std::runtime_error("Failed to create window");

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        throw std::runtime_error("Failed to initialize GLAD");

    glEnable(GL_DEPTH_TEST);
    createShaders();
    shader->use();

    // Set up viewport and clear color
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);

    // Set up axes
    //initAxes();

    // Set this instance as the user pointer for callbacks
    glfwSetWindowUserPointer(window, this);

    // Mouse callbacks
    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        (void)mods; // Unused parameter cast to avoid warnings
        double x, y;
        glfwGetCursorPos(win, &x, &y);
        // viewer is a pointer to the GcodeViewer instance
        auto* viewer = static_cast<GcodeViewer*>(glfwGetWindowUserPointer(win));
        if (viewer) viewer->onMouseButton(button, action, x, y);
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
        auto* viewer = static_cast<GcodeViewer*>(glfwGetWindowUserPointer(win));
        if (viewer) viewer->onMouseMove(x, y);
    });

    glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
        (void)xoffset; // Unused parameter cast to avoid warnings
        auto* viewer = static_cast<GcodeViewer*>(glfwGetWindowUserPointer(win));
        if (viewer) viewer->onScroll(yoffset);
    });

}

void GcodeViewer::createShaders() {
    shader = new Shader("shaders/gcode.vert", "shaders/gcode.frag");
}

void GcodeViewer::setupBuffers() {
    // Setup toolpath line VAO/VBO
    std::vector<float> vertices;
    for (const auto& pt : path) {
        vertices.push_back(pt.position.x);
        vertices.push_back(pt.position.y);
        vertices.push_back(pt.position.z);
    }
    pathVertexCount = vertices.size() / 3;

    glGenVertexArrays(1, &pathVAO);
    glGenBuffers(1, &pathVBO);

    glBindVertexArray(pathVAO);
    glBindBuffer(GL_ARRAY_BUFFER, pathVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void GcodeViewer::setToolPosition(const glm::vec3& pos) {
    toolPosition = pos;
}

bool GcodeViewer::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void GcodeViewer::pollEvents() {
    glfwPollEvents();
}

void GcodeViewer::drawFrame() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    view = getViewMatrix();
    projection = getProjectionMatrix();

    shader->setMat4("uProj", projection);
    shader->setMat4("uView", view);

    drawAxes();
    drawToolpath();
    drawToolhead();

    glfwSwapBuffers(window);
}

void GcodeViewer::drawToolpath() {
    glBindVertexArray(pathVAO);
    shader->setVec3("uColor", glm::vec3(0.0f, 1.0f, 1.0f));
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(pathVertexCount));
    glBindVertexArray(0);
}

// Mouse callback for camera control
void GcodeViewer::onMouseButton(int button, int action, double xpos, double ypos) {

  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    leftButtonDown = (action == GLFW_PRESS);
    std::cout << "leftButtonDown: " << leftButtonDown << std::endl;
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT) {
    rightButtonDown = (action == GLFW_PRESS);
  }
  lastMousePos = glm::vec2(xpos, ypos);
}

void GcodeViewer::onMouseMove(double xpos, double ypos) {
  glm::vec2 currentPos = glm::vec2(xpos, ypos);
  glm::vec2 delta = currentPos - lastMousePos;
  lastMousePos = currentPos;

  if (leftButtonDown) {
    // Rotate around target
    float sensitivity = 0.3f;
    yaw += delta.x * sensitivity;
    pitch += delta.y * sensitivity;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
  }

  if (rightButtonDown) {
    if (orthographicMode) {
      // In orthographic mode, pan based on view center and width (delta.y sign is inverted)
      viewCenter -= glm::vec2(delta.x, -delta.y) * (viewWidth / 400.0f); // Adjust based on window width
    } else {
      // In perspective mode, pan based on camera target and distance
      glm::vec3 up = glm::vec3(0, 1, 0);
      float panSpeed = cameraDistance * 0.002f;
      glm::vec3 right = glm::normalize(glm::cross(getCameraDirection(), up));
      cameraTarget -= right * delta.x * panSpeed;
      cameraTarget += up * delta.y * panSpeed;
    }
  }
}

void GcodeViewer::onScroll(double yoffset) {
  cameraDistance *= std::pow(0.9f, yoffset);  // Smooth zoom
  cameraDistance = glm::clamp(cameraDistance, 1.0f, 500.0f);
}

glm::vec3 GcodeViewer::getCameraDirection() {
  float pitchRad = glm::radians(pitch);
  float yawRad = glm::radians(yaw);
  return glm::normalize(glm::vec3(
    cos(pitchRad) * sin(yawRad),
    sin(pitchRad),
    cos(pitchRad) * cos(yawRad)
  ));
}

// view matrix
glm::mat4 GcodeViewer::getViewMatrix() {
  glm::vec3 direction = getCameraDirection();
  glm::vec3 cameraPos = cameraTarget - direction * cameraDistance;
  glm::vec3 up = glm::vec3(0, 1, 0);
  return glm::lookAt(cameraPos, cameraTarget, up);
}

// projection matrix
glm::mat4 GcodeViewer::getProjectionMatrix() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspect = static_cast<float>(width) / static_cast<float>(height);

    // Apply zoom factor to the base view width
    float zoomedWidth = viewWidth * (INITIAL_CAMERA_DISTANCE / cameraDistance);
    float halfWidth = zoomedWidth * 0.5f;
    float halfHeight = halfWidth / aspect;

    float left = viewCenter.x - halfWidth;
    float right = viewCenter.x + halfWidth;
    float bottom = viewCenter.y - halfHeight;
    float top = viewCenter.y + halfHeight;

    return glm::ortho(left, right, bottom, top, -1000.0f, 1000.0f);
}

void GcodeViewer::initAxes() {
    if (axesInitialized) return;

    // 3 axis segments, each from origin to positive direction
    std::vector<float> axesVertices = {
        0.0f, 0.0f, 0.0f,   AXES_LENGTH, 0.0f, 0.0f,  // X (red)
        0.0f, 0.0f, 0.0f,   0.0f, AXES_LENGTH, 0.0f,  // Y (green)
        0.0f, 0.0f, 0.0f,   0.0f, 0.0f, AXES_LENGTH   // Z (blue)
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

void GcodeViewer::drawAxes() {
    initAxes();
    
    glBindVertexArray(axesVAO);

    // Red X axis
    shader->setVec3("uColor", glm::vec3(1.0f, 0.0f, 0.0f));
    glDrawArrays(GL_LINES, 0, 2);

    // Green Y axis
    shader->setVec3("uColor", glm::vec3(0.0f, 1.0f, 0.0f));
    glDrawArrays(GL_LINES, 2, 2);

    // Blue Z axis
    shader->setVec3("uColor", glm::vec3(0.0f, 0.0f, 1.0f));
    glDrawArrays(GL_LINES, 4, 2);

    glBindVertexArray(0);

}

void GcodeViewer::initToolhead() {
    if (toolheadInitialized) return;

    std::vector<glm::vec3> vertices;

    for (int i = 0; i < SPHERE_STACKS; ++i) {
        float phi1 = glm::pi<float>() * float(i) / SPHERE_STACKS;
        float phi2 = glm::pi<float>() * float(i + 1) / SPHERE_STACKS;

        for (int j = 0; j <= SPHERE_SLICES; ++j) {
            float theta = 2 * glm::pi<float>() * float(j) / SPHERE_SLICES;

            float x1 = SPHERE_RADIUS * sin(phi1) * cos(theta);
            float y1 = SPHERE_RADIUS * sin(phi1) * sin(theta);
            float z1 = SPHERE_RADIUS * cos(phi1);

            float x2 = SPHERE_RADIUS * sin(phi2) * cos(theta);
            float y2 = SPHERE_RADIUS * sin(phi2) * sin(theta);
            float z2 = SPHERE_RADIUS * cos(phi2);

            vertices.emplace_back(x1, y1, z1);
            vertices.emplace_back(x2, y2, z2);
        }
    }

    toolheadVertexCount = vertices.size();

    glGenVertexArrays(1, &toolheadVAO);
    glGenBuffers(1, &toolheadVBO);

    glBindVertexArray(toolheadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, toolheadVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    toolheadInitialized = true;
}

void GcodeViewer::drawToolhead() {
    initToolhead();

    shader->use();
    shader->setVec3("uColor", glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat4 model = glm::translate(glm::mat4(1.0f), toolPosition);
    glm::mat4 mvp = projection * view * model;
    shader->setMat4("uProj", mvp);

    glBindVertexArray(toolheadVAO);

    int vertsPerStrip = (SPHERE_SLICES + 1) * 2;
    int numStrips = toolheadVertexCount / vertsPerStrip;

    for (int i = 0; i < numStrips; ++i) {
        glDrawArrays(GL_TRIANGLE_STRIP, i * vertsPerStrip, vertsPerStrip);
    }

    glBindVertexArray(0);
}

