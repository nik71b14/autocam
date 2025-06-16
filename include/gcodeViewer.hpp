#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "boolOps.hpp"
#include "gcode.hpp"
#include "gcode_params.hpp"
#include "shader.hpp"
#include "voxelizer.hpp"

// Enums 
enum class ProjectionType { ORTHOGRAPHIC, PERSPECTIVE };
enum class VOType { WORKPIECE, TOOL };

struct GLFWwindow;  // Forward declaration for GLFW window to avoid prbles with glad/glad.h

class GcodeViewer {
 public:
  GcodeViewer(const std::vector<GcodePoint>& toolpath);
  ~GcodeViewer();

  void pollEvents();
  void drawFrame();
  void carve(glm::vec3 pos);

  // Set Voxelized Objects (VO)
  void setWorkpieceVO(std::string workpiecePath) { initWorkpieceVO(workpiecePath.c_str()); };
  void setToolVO(std::string toolPath) { initToolVO(toolPath.c_str()); };

  void setToolPosition(const glm::vec3& pos);
  void setProjectionType(ProjectionType type) { projectionType = type; };

 private:
  void init();
  bool shouldClose() const;

  Shader* shader = nullptr;
  Shader* shader_flat = nullptr;
  Shader* shader_raymarching = nullptr;
  GLFWwindow* window = nullptr;
  glm::vec3 toolPosition;
  glm::mat4 projection, view;
  ProjectionType projectionType = ProjectionType::ORTHOGRAPHIC;  // Default to orthographic projection

  void createShaders();

  // Mouse management for camera control
  bool orthographicMode = true;                        // Use orthographic projection if true
  glm::vec3 cameraPosition = INITIAL_CAMERA_POSITION;  // Initial camera position
  glm::vec3 cameraTarget = INITIAL_CAMERA_TARGET;      // Point camera is looking at
  float cameraDistance = INITIAL_CAMERA_DISTANCE;      // Distance from camera to target (for zoom)
  float pitch = INITIAL_PITCH;                         // Up/down angle
  float yaw = INITIAL_YAW;                             // Left/right angle

  // Vars for orthographic projection
  glm::vec2 viewCenter = INITIAL_VIEW_CENTER;  // Center of orthographic projection
  float viewWidth = INITIAL_VIEW_WIDTH;        // Width of orthographic view

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
  // void setupBuffers();
  bool toolPathInitialized = false;
  void initToolpath();
  void drawToolpath();

  // Toolhead geometry (a sphere)
  GLuint toolheadVAO = 0;
  GLuint toolheadVBO = 0;
  int toolheadVertexCount = 0;
  bool toolheadInitialized = false;
  void initToolhead();
  void drawToolhead();

  // Workpiece geometry with normals (for shading)
  GLuint workpieceVAO = 0;
  GLuint workpieceVBO = 0;
  GLuint workpieceEBO = 0;
  int workpieceVertexCount = 0;
  bool workpieceInitialized = false;
  void initWorkpiece(const char* stlPath);
  void drawWorkpiece();

  // Tool geometry with normals (for shading)
  GLuint toolVAO = 0;
  GLuint toolVBO = 0;
  GLuint toolEBO = 0;
  int toolVertexCount = 0;
  bool toolInitialized = false;
  void initTool(const char* stlPath);
  void drawTool();

  // Boolean operations for voxel objects
  BoolOps* ops = nullptr;  // Boolean operations for voxel objects
  void initVO(const std::string& path, VOType type);

  // Workpiece VO
  VoxelizationParams params;
  GLuint workpieceVO_VAO = 0;
  GLuint workpieceVO_VBO = 0;
  GLuint workpieceVO_compressedBuffer = 0;
  GLuint workpieceVO_prefixSumBuffer = 0;
  void initWorkpieceVO(const std::string& path);
  void drawWorkpieceVO();

  // Tool VO
  void initToolVO(const std::string& path);

};
