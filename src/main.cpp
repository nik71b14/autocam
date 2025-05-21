#include <iostream>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <thread>

// Parses the OpenGL specification and generates code (usually .c and .h files)
// When your program starts, you call gladLoadGL() (or gladLoadGLLoader() for GLFW/Vulkan)
// GLAD loads all available OpenGL function pointers
// After that, you can just use OpenGL as if they were regular C functions
// glad.h and glad.c generated using https://glad.dav1d.de/, with settings: OpenGL, Core, 4.6, GLAD loader: glad, Language: C/C++, Extensions: GL_EXT_framebuffer_object, GL_EXT_texture_compression_s3tc, GL_EXT_texture_filter_anisotropic, GL_EXT_texture_sRGB_decode
#include <glad/glad.h>

// GLFW takes care of all the platform-specific boilerplate for setting up an OpenGL rendering environment.
// Instead of writing native Windows/macOS/Linux code to open a window, set up an OpenGL context, and handle input events,
// GLFW does all that in a cross-platform way.
#include <GLFW/glfw3.h>

// OpenGL math library, installed from MSYS2 shell with: pacman -S mingw-w64-ucrt-x86_64-glm (headers-only lib, files are installed in C:\msys64\ucrt64\include\glm
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

//  It’s a C++ library designed to import 3D model files, installed from MSYS2 shell with: pacman -S mingw-w64-ucrt-x86_64-assimp (Assimp headers in: C:/msys64/ucrt64/include/assimp, library in: C:/msys64/ucrt64/lib/libassimp.a
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "shader.hpp"
#include "meshLoader.hpp"
#include "glUtils.hpp"
#include "voxelizer.hpp"

// Constants ----------------------------------------------------------------
const char* STL_PATH = "models/model3_bin.stl";
//const char* STL_PATH = "models/cone.stl";
//const char* STL_PATH = "models/cube.stl";
//const char* STL_PATH = "models/single_face_xy.stl";
const int RESOLUTION = 2048;
const bool PREVIEW = false; // Set to false to disable preview rendering

// Global variables -----------------------------------------------------------
MeshBuffers meshBuffers;
GLFWwindow* window;
GLuint fbo, colorTex, depthRbo;
Shader* shader;
Shader* transitionShader;

int main(int argc, char** argv) {

    // if (argc < 2) {
    //     std::cerr << "Usage: " << argv[0] << " <path_to_stl_file>" << std::endl;
    //     return EXIT_FAILURE;
    // }

    const char* stlPath = (argc > 1) ? argv[1] : STL_PATH;

    try {
        setupGL(&window, RESOLUTION, RESOLUTION, "STL Viewer", !PREVIEW);
        if (!window) throw std::runtime_error("Failed to create GLFW window");

        std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
        std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

        queryGPULimits();

        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        float zSpan = 1.01f * loadMesh(stlPath, vertices, indices);

        std::cout << "Number of vertices: " << vertices.size() / 3 << std::endl;
        std::cout << "Number of faces: " << indices.size() / 3 << std::endl;

        meshBuffers = uploadMesh(vertices, indices);

        shader = new Shader("shaders/vertex_ok.glsl", "shaders/fragment_ok.glsl");
        transitionShader = new Shader("shaders/transitions.comp"); //###

        createFramebuffer(fbo, colorTex, depthRbo, RESOLUTION);

        auto startTime = std::chrono::high_resolution_clock::now();
        // --------------------------------------------------------------------
        voxelize(meshBuffers, indices.size(), zSpan, shader, transitionShader, window, fbo, colorTex, RESOLUTION, PREVIEW);
        // Mantains the context alive while the voxelization is running
        // while (!glfwWindowShouldClose(window)) {
        //     glfwPollEvents();
        // }
        // --------------------------------------------------------------------
        auto endTime = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> elapsedTime = endTime - startTime;
        std::cout << "Execution time: " << elapsedTime.count() << " seconds\n";

        // Clean up
        deleteMeshBuffers(meshBuffers);
        destroyFramebuffer(fbo, colorTex, depthRbo);
        delete shader;
        glfwDestroyWindow(window);
        glfwTerminate();

    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
