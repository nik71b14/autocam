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
#include "GLUtils.hpp"
#include "voxelizerZ.hpp"
#include "voxelizerZUtils.hpp"

// Constants ----------------------------------------------------------------
const char* STL_PATH = "models/model3_bin.stl";
//const char* STL_PATH = "models/cone.stl";
//const char* STL_PATH = "models/cube.stl";
//const char* STL_PATH = "models/single_face_xy.stl";

// Global variables -----------------------------------------------------------
MeshBuffers meshBuffers;
GLFWwindow* window;
GLuint fbo, colorTex, depthRbo;
Shader* drawShader;
Shader* transitionShader;
Shader* transitionShaderZ;


int main(int argc, char** argv) {

  const char* stlPath = (argc > 1) ? argv[1] : STL_PATH;

  try {

    VoxelizationParams params;
    params.resolution = 1024;
    params.resolutionZ = 1024; // Default Z resolution
    params.maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB

    setupGL(&window, params.resolution, params.resolution, "STL Viewer", !params.preview);
    if (!window) throw std::runtime_error("Failed to create GLFW window");

    // Get system information
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    queryGPULimits();

    params.slicesPerBlock = chooseOptimalPowerOfTwoSlicesPerBlock(params);
    //params.slicesPerBlock = chooseOptimalSlicesPerBlock(params.resolution, params.resolutionZ, params.maxMemoryBudgetBytes);
    std::cout << "Optimal slices per block (power of 2): " << params.slicesPerBlock << std::endl;

    std::size_t vram = getAvailableVRAM();
    std::cout << "Estimated available VRAM for 2D textures: " << (vram / (1024 * 1024)) << " MB\n";

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    float zSpan = 1.01f * loadMesh(stlPath, vertices, indices);

    std::cout << "Loaded mesh: " << stlPath << std::endl;
    std::cout << "Number of vertices: " << vertices.size() / 3 << std::endl;
    std::cout << "Number of faces: " << indices.size() / 3 << std::endl;

    meshBuffers = uploadMesh(vertices, indices);

    drawShader = new Shader("shaders/vertex_ok.glsl", "shaders/fragment_ok.glsl");
    transitionShader = new Shader("shaders/transitions.comp");
    transitionShaderZ = new Shader("shaders/transitions_z.comp");

    GLuint sliceTex, fbo;

    // Create 2D array texture to hold Z slices @@@MOVE TO createFramebufferZ
    glGenTextures(1, &sliceTex);
    glBindTexture(GL_TEXTURE_2D_ARRAY, sliceTex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, params.resolution, params.resolution, params.slicesPerBlock + 1);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Create framebuffer
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    voxelizeZ(
        meshBuffers,
        indices.size(),
        zSpan,
        drawShader,
        transitionShaderZ,
        fbo,
        sliceTex,
        params
    );

    // Clean up
    deleteMeshBuffers(meshBuffers);
    destroyFramebuffer(fbo, colorTex, depthRbo);
    delete drawShader;
    delete transitionShader;
    delete transitionShaderZ;
    glfwDestroyWindow(window);
    glfwTerminate();

  } catch (const std::exception& e) {
    std::cerr << "[Error] " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
