#include "visualizerZ.hpp"
#include "shader.hpp"
#include <glad/glad.h> // This before GLFW to avoid conflicts
#include <GLFW/glfw3.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

// VISUALIZATION: Render the voxelized mesh using raymarching ===============

  // Concept
  // 1. Implement a raymarching shader that casts rays through the voxel grid.
  // 2. For each (x, y) column, use the prefixBuffer to get how many transitions there are.
  // 3. Then, read the Z values from transitionsBuffer.
  // 4. Reconstruct the inside/outside intervals using the even-odd rule.

  // Upload buffers as 2D textures or SSBOs:
  //   prefixBuffer → 2D texture (or SSBO).
  //   transitionsBuffer → 3D texture or 2D texture with third dimension flattened.

  // Raymarching fragment shader:
  //   For each fragment ray (typically through a cube), transform the ray to voxel space.
  //   For each ray step:
  //   Map (x, y) to buffer index.
  //   Lookup transition count and Z intervals.
  //   Use even-odd filling to determine if current Z is inside or outside.

  // Color according to state:
  //   Inside = none
  //   Outside = gray
  //   Transition surfaces = highlight

  // It is supposed that i have the following buffers ready for the raymarching shader:
  //    - A compressedBuffer contains the Z transition indices for each XY column.
  //    - A prefixSumBuffer contains the start index in the compressedBuffer for each XY column (exclusive prefix sum).
  //    - Resolution is RESOLUTION, RESOLUTION, and RESOLUTION_Z (set appropriately).
  //    - Voxel space origin is normalized to [0,1].

  // - InitWindow() initializes GLFW, creates a window and context, loads GLAD.
  // - RenderFullScreenQuad() creates a VAO/VBO for a simple two-triangle fullscreen quad and renders it.
  // - The main loop clears the screen, binds buffers, sets uniforms, and draws the fullscreen quad.
  // - You must have compressedBuffer and prefixSumBuffer created and filled beforehand.
  // - This minimal example assumes your shader files are in the shaders/ folder.
  // - Adjust uniforms and binding points as per your shader code.
  // - You can expand this to handle inputs, resizing, camera movement, etc.

  // Assume compressedBuffer and prefixSumBuffer are already filled and valid

// Forward declarations
void RenderFullScreenQuadx(GLuint &quadVAO, Shader* raymarchingShader);
GLFWwindow* InitWindowx(int width, int height, const char* title);
bool loadBinaryFilex(const std::string& filename, std::vector<unsigned int>& outData);

void visualizeZ(
  const std::string& compressedBufferFile,
  const std::string& prefixSumBufferFile,
  int resolutionXY,
  int resolutionZ,
  int maxTransitionsPerZColumn) {

  // Load and compile the shader program
  Shader* raymarchingShader;

  // Global VAO/VBO for fullscreen quad
  GLuint quadVAO = 0;
  GLuint quadVBO = 0;

  const int windowWidth = 800;
  const int windowHeight = 600;

  GLFWwindow* window = InitWindowx(windowWidth, windowHeight, "Voxel Transition Viewer");
  if (!window) {
    glfwTerminate();
    throw std::runtime_error("Failed to create GLFW window for voxel transition viewer");
  }

  // Call this after creating the window (particularly after the functions GLFWwindow* window = glfwCreateWindow(...) and glfwMakeContextCurrent(window) have been called)
  raymarchingShader = new Shader("shaders/raymarching.vert", "shaders/raymarching.frag");
  raymarchingShader->use();
  
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);

  // Setup a fullscreen quad VAO/VBO if not already created (using two triangles)
  if (quadVAO == 0) {
    float quadVertices[] = {
        // positions
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
        -1.0f,  1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
  }

  std::vector<unsigned int> compressedData;
  std::vector<unsigned int> prefixSumData;

  bool success1 = loadBinaryFilex(compressedBufferFile, compressedData);
  bool success2 = loadBinaryFilex(prefixSumBufferFile, prefixSumData);

  if (success1 && success2) {
      std::cout << "Successfully loaded buffers." << std::endl;
      std::cout << "Compressed buffer size: " << compressedData.size() << std::endl;
      std::cout << "Prefix sum buffer size: " << prefixSumData.size() << std::endl;
  }

  // Create two SSBO buffers and load them with the data from compressedData and prefixSumData
  GLuint compressedBuffer, prefixSumBuffer;
  glGenBuffers(1, &compressedBuffer);
  glGenBuffers(1, &prefixSumBuffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, compressedBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, compressedData.size() * sizeof(GLuint), compressedData.data(), GL_DYNAMIC_COPY); // Allocate and upload compressed data
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, prefixSumBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, prefixSumData.size() * sizeof(GLuint), prefixSumData.data(), GL_DYNAMIC_COPY); // Allocate and upload prefix sum data
  
  // Output the size of the two generated buffers
  std::cout << "Compressed buffer size (elements): " << compressedData.size() << std::endl;
  std::cout << "Prefix sum buffer size (elements): " << prefixSumData.size() << std::endl;
  
  // Bind the buffers to the shader storage binding points
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, compressedBuffer);  // TransitionBuffer
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, prefixSumBuffer);   // PrefixSumBuffer

  // Render loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Clear screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set uniforms
    raymarchingShader->setIVec3("resolution", glm::ivec3(resolutionXY, resolutionXY, resolutionZ)); //&&&&&&&&
    //raymarchingShader->setIVec3("resolution", glm::ivec3(200, 200, 200));
    raymarchingShader->setInt("maxTransitions", maxTransitionsPerZColumn);

    float fov = 45.0f;
    float aspectRatio = float(windowWidth) / float(windowHeight);

    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 2.0f);
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(cameraPos, cameraTarget, cameraUp);
    glm::mat4 invViewProj = glm::inverse(proj * view);

    raymarchingShader->setMat4("invViewProj", invViewProj);
    raymarchingShader->setVec3("cameraPos", cameraPos);
    raymarchingShader->setIVec2("screenResolution", glm::ivec2(windowWidth, windowHeight));

    // Draw fullscreen quad
    RenderFullScreenQuadx(quadVAO, raymarchingShader);

    glfwSwapBuffers(window);
  }

  // Cleanup
  glDeleteBuffers(1, &prefixSumBuffer);
  glDeleteBuffers(1, &compressedBuffer);
  glDeleteBuffers(1, &quadVBO);
  delete raymarchingShader;
  glDeleteVertexArrays(1, &quadVAO);
  
  glfwDestroyWindow(window);
}

void RenderFullScreenQuadx(GLuint &quadVAO, Shader* raymarchingShader) {
  glBindVertexArray(quadVAO);
  raymarchingShader->use(); //&&& SOLO PER DEBUG, ELIMINARE!

  // GLint currentProgram;
  // glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
  // std::cout << "Current shader program ID: " << currentProgram << std::endl;

  glDrawArrays(GL_TRIANGLES, 0, 6);
}

// Create a GLFW window and initialize GLAD (GLFW = GL Framework, GLAD = OpenGL loader)
GLFWwindow* InitWindowx(int width, int height, const char* title) {
  // if (!glfwInit()) {
  //     std::cerr << "Failed to initialize GLFW\n";
  //     return nullptr;
  // }
  if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");

  // OpenGL 4.6 Core Profile
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4); // OpenGL version 4.6
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Use Core Profile, i.e. without deprecated features
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // Allow resizing the window
  glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE); // Make the window visible

  GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
  // if (!window) {
  //     std::cerr << "Failed to create GLFW window\n";
  //     glfwTerminate();
  //     return nullptr;
  // }
  if (!window) throw std::runtime_error("Failed to create GLFW window");

  glfwMakeContextCurrent(window);

  // Load OpenGL function pointers using GLAD
  // if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
  //     std::cerr << "Failed to initialize GLAD\n";
  //     glfwDestroyWindow(window);
  //     glfwTerminate();
  //     return nullptr;
  // }

  // Load OpenGL function pointers using GLAD
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
  throw std::runtime_error("Failed to initialize GLAD");

  // Enable vsync
  glfwSwapInterval(1);

  // Set viewport
  glViewport(0, 0, width, height);

  return window;
}

bool loadBinaryFilex(const std::string& filename, std::vector<unsigned int>& outData) {
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
      std::cerr << "Error: Cannot open file " << filename << std::endl;
      return false;
  }

  file.seekg(0, std::ios::end);
  std::streamsize fileSize = file.tellg();
  if (fileSize % sizeof(unsigned int) != 0) {
      std::cerr << "Error: File size of " << filename << " is not a multiple of sizeof(unsigned int)." << std::endl;
      return false;
  }

  file.seekg(0, std::ios::beg);
  size_t numElements = fileSize / sizeof(unsigned int);
  outData.resize(numElements);

  file.read(reinterpret_cast<char*>(outData.data()), fileSize);
  if (!file) {
      std::cerr << "Error: Only read " << file.gcount() << " bytes from " << filename << std::endl;
      return false;
  }

  return true;
}