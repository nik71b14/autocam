#include "voxelViewer.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

VoxelViewer::VoxelViewer(const std::string& compressedFile,
                                             const std::string& prefixSumFile,
                                             int resXY, int resZ, int maxTransitions)
    : resolutionXY(resXY), resolutionZ(resZ), maxTransitionsPerZColumn(maxTransitions) {
    if (!loadBinaryFile(compressedFile, compressedData) ||
        !loadBinaryFile(prefixSumFile, prefixSumData)) {
        throw std::runtime_error("Failed to load one or both input files");
    }
    initGL();
    setupShaderAndBuffers();
}

VoxelViewer::VoxelViewer(const std::vector<unsigned int>& compressed,
                                             const std::vector<unsigned int>& prefixSum,
                                             int resXY, int resZ, int maxTransitions)
    : resolutionXY(resXY), resolutionZ(resZ), maxTransitionsPerZColumn(maxTransitions),
      compressedData(compressed), prefixSumData(prefixSum) {
    initGL();
    setupShaderAndBuffers();
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

    window = glfwCreateWindow(800, 600, "Voxel Transition Viewer", nullptr, nullptr);
    if (!window) throw std::runtime_error("Failed to create GLFW window");

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        throw std::runtime_error("Failed to initialize GLAD");

    glfwSwapInterval(1);
    glViewport(0, 0, 800, 600);
}

void VoxelViewer::setupShaderAndBuffers() {
    raymarchingShader = new Shader("shaders/raymarching.vert", "shaders/raymarching.frag");
    raymarchingShader->use();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    float quadVertices[] = {
        -1.0f, -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f, -1.0f,   1.0f,  1.0f,
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
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        raymarchingShader->use();
        raymarchingShader->setIVec3("resolution", glm::ivec3(resolutionXY, resolutionXY, resolutionZ));
        raymarchingShader->setInt("maxTransitions", maxTransitionsPerZColumn);

        float aspect = 800.0f / 600.0f;
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0,0,2), glm::vec3(0), glm::vec3(0,1,0));
        glm::mat4 invViewProj = glm::inverse(proj * view);

        raymarchingShader->setMat4("invViewProj", invViewProj);
        raymarchingShader->setVec3("cameraPos", glm::vec3(0,0,2));
        raymarchingShader->setIVec2("screenResolution", glm::ivec2(800, 600));

        renderFullScreenQuad();
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
