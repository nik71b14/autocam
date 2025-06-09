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
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float left = -100.0f;
    float right = 100.0f;
    float halfVertical = (abs(left) + abs(right)) / 2 / aspect; //50.0f / aspect;
    std::cout << "aspect: " << aspect << std::endl;
    std::cout << "halfVertical: " << halfVertical << std::endl;
    float bottom = -halfVertical;
    float top = halfVertical;

    projection = glm::ortho(left, right, bottom, top, -1000.0f, 1000.0f);
    view = glm::lookAt(glm::vec3(0, 0, 100), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

}

void GcodeViewer::drawFrame() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader->use();
    shader->setMat4("uProj", projection);
    shader->setMat4("uView", view);

    drawToolpath();
    drawToolhead();

    glfwSwapBuffers(window);
}

void GcodeViewer::drawToolpath() {
    std::cout << "pathVertexCount: " << pathVertexCount << std::endl;
    glBindVertexArray(pathVAO);
    shader->setVec3("uColor", glm::vec3(0.0f, 1.0f, 1.0f));
    glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(pathVertexCount));
    glBindVertexArray(0);
}

void GcodeViewer::drawToolhead() {
    shader->use();
    shader->setVec3("uColor", glm::vec3(1.0f, 0.0f, 0.0f));

    glm::mat4 model = glm::translate(glm::mat4(1.0f), toolPosition);
    glm::mat4 mvp = projection * view * model;
    shader->setMat4("uProj", mvp);

    const int stacks = 12;
    const int slices = 24;
    const float radius = 1.0f;

    std::vector<glm::vec3> vertices;

    for (int i = 0; i < stacks; ++i) {
        float phi1 = glm::pi<float>() * float(i) / stacks;
        float phi2 = glm::pi<float>() * float(i + 1) / stacks;

        for (int j = 0; j <= slices; ++j) {
            float theta = 2 * glm::pi<float>() * float(j) / slices;

            float x1 = radius * sin(phi1) * cos(theta);
            float y1 = radius * sin(phi1) * sin(theta);
            float z1 = radius * cos(phi1);

            float x2 = radius * sin(phi2) * cos(theta);
            float y2 = radius * sin(phi2) * sin(theta);
            float z2 = radius * cos(phi2);

            vertices.emplace_back(x1, y1, z1);
            vertices.emplace_back(x2, y2, z2);
        }
    }

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);

    int vertsPerStrip = (slices + 1) * 2;
    for (int i = 0; i < stacks; ++i) {
        glDrawArrays(GL_TRIANGLE_STRIP, i * vertsPerStrip, vertsPerStrip);
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}


