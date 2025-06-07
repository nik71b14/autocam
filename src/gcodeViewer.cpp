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
    updateCamera();
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

void GcodeViewer::updateCamera() {
    projection = glm::perspective(glm::radians(45.0f), 800.f / 600.f, 0.1f, 1000.0f);
    view = glm::lookAt(glm::vec3(50, 50, 100), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
}

void GcodeViewer::drawFrame() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProj"), 1, GL_FALSE, &projection[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uView"), 1, GL_FALSE, &view[0][0]);

    drawToolpath();
    drawToolhead();

    glfwSwapBuffers(window);
}

void GcodeViewer::drawToolpath() {
    glBindVertexArray(pathVAO);
    glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), 0.0f, 1.0f, 1.0f);
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(pathVertexCount));
    glBindVertexArray(0);
}

void GcodeViewer::drawToolhead() {
    glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), 1.0f, 0.0f, 0.0f);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), toolPosition);
    glm::mat4 mvp = projection * view * model;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProj"), 1, GL_FALSE, &mvp[0][0]);

    // Draw point as toolhead
    float pos[] = { 0.f, 0.f, 0.f };
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_POINTS, 0, 1);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}
