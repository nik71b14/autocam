#include "gcodeViewer.hpp"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <iostream>
#include <stdexcept>

#include "boolOps.hpp"
#include "meshLoader.hpp"
#include "shader.hpp"

GcodeViewer::GcodeViewer(GLFWwindow* window, const std::vector<GcodePoint>& toolpath) : window(window), toolPosition(0.0f), path(toolpath) { init(); }

GcodeViewer::~GcodeViewer() {
  // Cleanup
  if (shader) delete shader;
  if (shader_flat) delete shader_flat;
  if (shader_raymarching) delete shader_raymarching;
  if (window) glfwSetWindowUserPointer(window, nullptr);  // Clear user pointer
  if (axesVAO) glDeleteVertexArrays(1, &axesVAO);
  if (axesVBO) glDeleteBuffers(1, &axesVBO);
  if (pathVAO) glDeleteVertexArrays(1, &pathVAO);
  if (pathVBO) glDeleteBuffers(1, &pathVBO);
  if (toolheadVAO) glDeleteVertexArrays(1, &toolheadVAO);
  if (toolheadVBO) glDeleteBuffers(1, &toolheadVBO);
  if (workpieceVAO) glDeleteVertexArrays(1, &workpieceVAO);
  if (workpieceVBO) glDeleteBuffers(1, &workpieceVBO);
  if (workpieceEBO) glDeleteBuffers(1, &workpieceEBO);
  if (toolVAO) glDeleteVertexArrays(1, &toolVAO);
  if (toolVBO) glDeleteBuffers(1, &toolVBO);
  if (toolEBO) glDeleteBuffers(1, &toolEBO);
  if (workpieceVO_VAO) glDeleteVertexArrays(1, &workpieceVO_VAO);
  if (workpieceVO_VBO) glDeleteBuffers(1, &workpieceVO_VBO);
  if (workpieceVO_compressedBuffer) glDeleteBuffers(1, &workpieceVO_compressedBuffer);
  if (workpieceVO_prefixSumBuffer) glDeleteBuffers(1, &workpieceVO_prefixSumBuffer);
}

void GcodeViewer::checkContext() {
  if (!window || glfwGetCurrentContext() != window) {
    throw std::runtime_error("GLFW window is null or OpenGL context is not current for this window.");
  }
}

void GcodeViewer::init() {
  checkContext();

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(CLEAR_COLOR);
  glfwSetWindowSize(window, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);

  createShaders();
  setMouseCallbacks();

  // Lighting setup
  shader->use();
  shader->setVec3("lightDir", LIGHT_DIRECTION);
}

void GcodeViewer::createShaders() {
  checkContext();

  shader = new Shader("shaders/gcode.vert", "shaders/gcode.frag");
  shader_flat = new Shader("shaders/gcode_flat.vert", "shaders/gcode_flat.frag");
  shader_raymarching = new Shader("shaders/raymarching.vert", "shaders/raymarching.frag");
}

void GcodeViewer::setMouseCallbacks() {
  checkContext();

  // Set this instance as the user pointer for callbacks
  glfwSetWindowUserPointer(window, this);

  // Set viewport size
  glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int width, int height) {
    (void)win;  // Unused parameter cast to avoid warnings
    glViewport(0, 0, width, height);
  });

  // Mouse callbacks
  glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
    (void)mods;  // Unused parameter cast to avoid warnings
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
    (void)xoffset;  // Unused parameter cast to avoid warnings
    auto* viewer = static_cast<GcodeViewer*>(glfwGetWindowUserPointer(win));
    if (viewer) viewer->onScroll(yoffset);
  });
}

void GcodeViewer::initToolpath() {
  if (toolPathInitialized) return;

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

  toolPathInitialized = true;
}

void GcodeViewer::setToolPosition(const glm::vec3& pos) { toolPosition = pos; }

bool GcodeViewer::shouldClose() const { return glfwWindowShouldClose(window); }

void GcodeViewer::pollEvents() { glfwPollEvents(); }

void GcodeViewer::drawFrame() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // glFrontFace(GL_CW);
  glFrontFace(GL_CCW);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glLineWidth(2.0f);
  glPointSize(5.0f);
  glEnable(GL_DEPTH_TEST);
  // glDepthFunc(GL_LEQUAL);
  // glDisable(GL_CULL_FACE);
  // glDisable(GL_BLEND); // Disable transparency

  view = getViewMatrix();
  projection = getProjectionMatrix();

  // Default model matrix for static objects
  shader->use();
  shader->setMat4("uProj", projection);
  shader->setMat4("uView", view);
  shader->setMat4("uModel", IDENTITY_MODEL);  // Identity model matrix for static objects

  // Default shader for flat objects
  shader_flat->use();
  shader_flat->setMat4("uProj", projection);
  shader_flat->setMat4("uView", view);
  shader_flat->setMat4("uModel", IDENTITY_MODEL);  // Identity model matrix for static objects

  glDepthMask(GL_FALSE);  // Don't write to depth buffer
  drawWorkpieceVO();      // shader_raymarching (voxelized object)
  glDepthMask(GL_TRUE);   // Re-enable depth writes

  drawAxes();      // shader_flat
  drawToolpath();  // shader_flat
  drawTool();      // shader (stl source)
  // drawWorkpiece();            // shader (stl source)

  glfwSwapBuffers(window);
}

void GcodeViewer::drawToolpath() {
  initToolpath();

  shader_flat->use();

  glBindVertexArray(pathVAO);
  shader_flat->setVec4("uColor", TOOLPATH_COLOR);
  glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(pathVertexCount));
  glBindVertexArray(0);
}

void GcodeViewer::onMouseButton(int button, int action, double xpos, double ypos) {
  if (button == GLFW_MOUSE_BUTTON_LEFT) {
    leftButtonDown = (action == GLFW_PRESS);
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
    yaw -= delta.x * sensitivity;
    pitch -= delta.y * sensitivity;
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
  }

  if (rightButtonDown) {
    if (orthographicMode) {
      // In orthographic mode, pan based on view center and width (delta.y sign is inverted)
      viewCenter -= glm::vec2(delta.x, -delta.y) * (viewWidth / 400.0f);  // Adjust based on window width
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
  return glm::normalize(glm::vec3(cos(pitchRad) * sin(yawRad), sin(pitchRad), cos(pitchRad) * cos(yawRad)));
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
      0.0f, 0.0f, 0.0f, AXES_LENGTH, 0.0f,        0.0f,        // X (red)
      0.0f, 0.0f, 0.0f, 0.0f,        AXES_LENGTH, 0.0f,        // Y (green)
      0.0f, 0.0f, 0.0f, 0.0f,        0.0f,        AXES_LENGTH  // Z (blue)
  };

  glGenVertexArrays(1, &axesVAO);          // This is the container for all the vertex attributes
  glGenBuffers(1, &axesVBO);               // This is the buffer that holds the vertex data
  glBindVertexArray(axesVAO);              // Bind the VAO to set up the vertex attributes
  glBindBuffer(GL_ARRAY_BUFFER, axesVBO);  // Bind the VBO to the VAO
  glBufferData(GL_ARRAY_BUFFER, axesVertices.size() * sizeof(float), axesVertices.data(), GL_STATIC_DRAW);  // Upload vertex data to the VBO
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);  // Vertex attribute at location 0 (read from shader as vec3 at location 0)
  glEnableVertexAttribArray(0);                                                 // Enable the vertex attribute at location 0
  glBindVertexArray(0);                                                         // Unbind the VAO to avoid accidental modifications

  axesInitialized = true;
}

void GcodeViewer::drawAxes() {
  initAxes();

  shader_flat->use();

  glBindVertexArray(axesVAO);

  // Red X axis
  shader_flat->setVec4("uColor", RED_COLOR);
  glDrawArrays(GL_LINES, 0, 2);

  // Green Y axis
  shader_flat->setVec4("uColor", GREEN_COLOR);
  glDrawArrays(GL_LINES, 2, 2);

  // Blue Z axis
  shader_flat->setVec4("uColor", BLUE_COLOR);
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
  shader->setVec4("uColor", TOOL_COLOR);

  // Pass the model matrix to the shader
  glm::mat4 model = glm::translate(IDENTITY_MODEL, toolPosition);
  shader->setMat4("uModel", model);

  glBindVertexArray(toolheadVAO);

  int vertsPerStrip = (SPHERE_SLICES + 1) * 2;
  int numStrips = toolheadVertexCount / vertsPerStrip;

  for (int i = 0; i < numStrips; ++i) {
    glDrawArrays(GL_TRIANGLE_STRIP, i * vertsPerStrip, vertsPerStrip);
  }

  glBindVertexArray(0);
}

// Init workpiece considering normals
void GcodeViewer::initWorkpiece(const char* stlPath) {
  if (workpieceInitialized) {
    return;
  }

  MeshWithNormals mesh = loadMeshWithNormals(stlPath);  // Assumes mesh has .vertices and .indices

  std::vector<float> vertices = mesh.vertices;  // flat array of floats (x,y,z,nx,ny,nz,x,y,z,nx,ny,nz,...)
  std::vector<unsigned int> indices = mesh.indices;

  // Create VAO, VBO, and EBO for the workpiece with normals
  glGenVertexArrays(1, &workpieceVAO);
  glGenBuffers(1, &workpieceVBO);
  glGenBuffers(1, &workpieceEBO);

  glBindVertexArray(workpieceVAO);

  glBindBuffer(GL_ARRAY_BUFFER, workpieceVBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, workpieceEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

  // Position attribute (location = 0)
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

  // Normal attribute (location = 1)
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

  glBindVertexArray(0);

  // Store index count
  workpieceVertexCount = static_cast<int>(vertices.size()) / 6;  // 6 floats per vertex (x,y,z,nx,ny,nz)
  workpieceInitialized = true;

#ifdef DEBUG_OUTPUT_GCODE
  std::cout << "Workpiece2 initialized with " << workpieceVertexCount << " vertices." << std::endl;
#endif
}

void GcodeViewer::drawWorkpiece() {
  initWorkpiece(WORKPIECE_STL_PATH);  // Replace with actual STL path

  shader->use();
  shader->setMat4("uModel", IDENTITY_MODEL);   // or transform if needed
  shader->setVec4("uColor", WORKPIECE_COLOR);  // base color

  glBindVertexArray(workpieceVAO);
  glDrawArrays(GL_TRIANGLES, 0, workpieceVertexCount);
  glBindVertexArray(0);
}

// Init tool considering normals
void GcodeViewer::initTool(const char* stlPath) {
  if (toolInitialized) {
    return;
  }

  MeshWithNormals mesh = loadMeshWithNormals(stlPath);  // Assumes mesh has .vertices and .indices

  std::vector<float> vertices = mesh.vertices;  // flat array of floats (x,y,z,nx,ny,nz,x,y,z,nx,ny,nz,...)
  std::vector<unsigned int> indices = mesh.indices;

  // Create VAO, VBO, and EBO for the tool with normals
  glGenVertexArrays(1, &toolVAO);
  glGenBuffers(1, &toolVBO);
  glGenBuffers(1, &toolEBO);

  glBindVertexArray(toolVAO);

  glBindBuffer(GL_ARRAY_BUFFER, toolVBO);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, toolEBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

  // Position attribute (location = 0)
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

  // Normal attribute (location = 1)
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

  glBindVertexArray(0);

  // Store index count
  toolVertexCount = static_cast<int>(vertices.size()) / 6;  // 6 floats per vertex (x,y,z,nx,ny,nz)
  toolInitialized = true;

#ifdef DEBUG_OUTPUT_GCODE
  std::cout << "Tool2 initialized with " << toolVertexCount << " vertices." << std::endl;
#endif
}

void GcodeViewer::drawTool() {
  initTool(TOOL_STL_PATH);  // Replace with actual STL path

  shader->use();
  shader->setVec4("uColor", TOOL_COLOR);

  // Pass the model matrix to the shader
  glm::mat4 model = glm::translate(IDENTITY_MODEL, toolPosition);
  shader->setMat4("uModel", model);

  glBindVertexArray(toolVAO);
  glDrawArrays(GL_TRIANGLES, 0, toolVertexCount);
  glBindVertexArray(0);
}

void GcodeViewer::setWorkpiece(std::string workpiecePath) { initVO(workpiecePath, VOType::WORKPIECE); }

void GcodeViewer::setTool(std::string toolPath) { initVO(toolPath, VOType::TOOL); }

void GcodeViewer::initVO(const std::string& path, VOType type) {
  //@@@ Tool management
  if (type == VOType::TOOL) {
    if (ops.getObjects().size() > 1) {
      std::cerr << "Tool already initialized. Only one tool can be set." << std::endl;
      return;
    }
    if (ops.getObjects().size() == 0) {
      std::cerr << "Set workpiece first." << std::endl;
      return;
    }
    if (!ops.load(path)) {
      std::cerr << "Failed to load tool object." << std::endl;
      return;
    }

    std::cout << "Tool loaded: " << path << std::endl;

    ops.subtractGPU_init(ops.getObjects()[0], ops.getObjects()[1]);  //@@@ MOVE TO A MORE SUITED POSITION TO ALLOW RESET, TOOL CHANGE, ETC.

    return;
  }

  if (!ops.load(path)) {
    std::cerr << "Failed to load voxelized object." << std::endl;
    return;
  }

#ifdef DEBUG_OUTPUT_GCODE
  std::cout << "Voxel Params:" << std::endl;
  std::cout << "  resolutionXYZ: (" << params.resolutionXYZ.x << ", " << params.resolutionXYZ.y << ", " << params.resolutionXYZ.z << ")" << std::endl;
  std::cout << "  maxTransitionsPerZColumn: " << params.maxTransitionsPerZColumn << std::endl;
  std::cout << "  zSpan: " << params.zSpan << std::endl;
  std::cout << "  scale: " << params.scale << std::endl;
  std::cout << "  color: (" << params.color.x << ", " << params.color.y << ", " << params.color.z << ")" << std::endl;
#endif

  // If the type is WORKPIECE, setup for visualization
  if (type == VOType::WORKPIECE) {
    VoxelObject obj = ops.getObjects().back();  // Get the last object loaded (assumed to be the workpiece)

    // Extract workpiece object data
    params = obj.params;  // Get voxelization parameters from the last object

    shader->use();
    shader_flat->use();
    shader_raymarching->use();

    // Clean all buffers and VAOs
    if (workpieceVO_VAO) glDeleteVertexArrays(1, &workpieceVO_VAO);
    if (workpieceVO_VBO) glDeleteBuffers(1, &workpieceVO_VBO);
    if (workpieceVO_compressedBuffer) glDeleteBuffers(1, &workpieceVO_compressedBuffer);
    if (workpieceVO_prefixSumBuffer) glDeleteBuffers(1, &workpieceVO_prefixSumBuffer);
    workpieceVO_VAO = 0;
    workpieceVO_VBO = 0;
    workpieceVO_compressedBuffer = 0;
    workpieceVO_prefixSumBuffer = 0;

    std::vector<unsigned int> compressedData;
    std::vector<unsigned int> prefixSumData;

    compressedData = obj.compressedData;  // Get compressed data from the first object
    prefixSumData = obj.prefixSumData;    // Get prefix sum data from the first object

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    float quadVertices[] = {
        -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
    };

    glGenVertexArrays(1, &workpieceVO_VAO);
    glGenBuffers(1, &workpieceVO_VBO);
    glBindVertexArray(workpieceVO_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, workpieceVO_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glGenBuffers(1, &workpieceVO_compressedBuffer);
    glGenBuffers(1, &workpieceVO_prefixSumBuffer);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, workpieceVO_compressedBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, compressedData.size() * sizeof(GLuint), compressedData.data(), GL_DYNAMIC_COPY);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, workpieceVO_prefixSumBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, prefixSumData.size() * sizeof(GLuint), prefixSumData.data(), GL_DYNAMIC_COPY);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, workpieceVO_compressedBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, workpieceVO_prefixSumBuffer);

    return;
  }

  if (type == VOType::TOOL) {
  }
}

void GcodeViewer::drawWorkpieceVO() {
  // initQuad(); //%%%%%

  glm::vec3 direction = getCameraDirection();
  glm::vec3 cameraPos = cameraTarget - direction * cameraDistance;
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);

  shader_raymarching->use();

  shader_raymarching->setIVec3("resolution", glm::ivec3(params.resolutionXYZ.x, params.resolutionXYZ.y, params.resolutionXYZ.z));
  shader_raymarching->setInt("maxTransitions", params.maxTransitionsPerZColumn);
  shader_raymarching->setFloat("normalizedZSpan", params.zSpan);

  // Compute model matrix, scaling back based on params.scale
  glm::mat4 model = glm::mat4(1.0f);
  if (this->projectionType == ProjectionType::ORTHOGRAPHIC) {
    model = glm::scale(model, glm::vec3(1.0f / params.scale));
    model = glm::translate(model, params.center * params.scale);  // Put the object at the correct, original position
  }

  glm::mat4 viewProj = projection * view * model;
  glm::mat4 invViewProj = glm::inverse(viewProj);

  shader_raymarching->setMat4("viewProj", viewProj);  // <-- NEW: set viewProj matrix
  shader_raymarching->setMat4("invViewProj", invViewProj);

  shader_raymarching->setVec3("cameraPos", cameraPos);
  shader_raymarching->setIVec2("screenResolution", glm::ivec2(width, height));
  shader_raymarching->setVec3("color", params.color);

  glBindVertexArray(workpieceVO_VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  shader_raymarching->dismiss();  // Dismiss the shader after use
}

void GcodeViewer::initWorkpieceVO(const std::string& path) { initVO(path, VOType::WORKPIECE); }

void GcodeViewer::initToolVO(const std::string& path) { initVO(path, VOType::TOOL); }

void GcodeViewer::carve(glm::vec3 pos) {
  //@@@ DEBUG
  // std::cout << "Carve position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;

  glm::vec3 carvePosition = pos - glm::vec3(0.0f, 0.0f, 0.0f);  //@@@ DEBUG Adjust position to center the tool
  ops.subtractGPU(carvePosition);

  //@@@ DEBUG: Increment a counter to track the number of carvings
  carvingCounter++;
  std::cout << "Counter: " << carvingCounter << std::endl;

  //@@@ DEBUG: stop here (no need to update the workpieceVO_VAO for now)
  return;

  // Update the voxelized workpiece object after carving
  const auto& obj = ops.getObjects()[0];
  params = obj.params;

  if (workpieceVO_VAO) glDeleteVertexArrays(1, &workpieceVO_VAO);
  if (workpieceVO_VBO) glDeleteBuffers(1, &workpieceVO_VBO);
  if (workpieceVO_compressedBuffer) glDeleteBuffers(1, &workpieceVO_compressedBuffer);
  if (workpieceVO_prefixSumBuffer) glDeleteBuffers(1, &workpieceVO_prefixSumBuffer);
  workpieceVO_VAO = 0;
  workpieceVO_VBO = 0;
  workpieceVO_compressedBuffer = 0;
  workpieceVO_prefixSumBuffer = 0;

  shader_raymarching->use();

  float quadVertices[] = {
      -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
  };

  glGenVertexArrays(1, &workpieceVO_VAO);
  glGenBuffers(1, &workpieceVO_VBO);
  glBindVertexArray(workpieceVO_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, workpieceVO_VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

  glGenBuffers(1, &workpieceVO_compressedBuffer);
  glGenBuffers(1, &workpieceVO_prefixSumBuffer);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, workpieceVO_compressedBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, obj.compressedData.size() * sizeof(GLuint), obj.compressedData.data(), GL_DYNAMIC_COPY);

  glBindBuffer(GL_SHADER_STORAGE_BUFFER, workpieceVO_prefixSumBuffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, obj.prefixSumData.size() * sizeof(GLuint), obj.prefixSumData.data(), GL_DYNAMIC_COPY);

  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, workpieceVO_compressedBuffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, workpieceVO_prefixSumBuffer);
}