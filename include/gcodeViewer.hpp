#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#include "boolOps.hpp"
#include "coordinateSystem.hpp"
#include "gcode.hpp"
#include "gcode_params.hpp"
#include "shader.hpp"
#include "voxelizer.hpp"

// Enums
enum class ProjectionType { ORTHOGRAPHIC, PERSPECTIVE };
enum class VOType { WORKPIECE, TOOL };

// How to interpret the numeric X/Y/Z in the G-code program:
//   MM    - millimetres in world space (canonical, physically correct). Converted
//           to stock voxels host-side via the stock CoordinateSystem; requires the
//           tool and stock to share a voxel size.
//   VOXEL - raw stock voxel indices (legacy). 1 G-code unit == 1 stock voxel. Kept
//           for the original sample programs (e.g. gcode/square_600.gcode).
enum class GcodeUnits { MM, VOXEL };

struct GLFWwindow;  // Forward declaration for GLFW window to avoid prbles with glad/glad.h

class GcodeViewer {
 public:
  GcodeViewer(GLFWwindow* window, const std::vector<GcodePoint>& toolpath);
  ~GcodeViewer();

  void pollEvents();
  void drawFrame();
  void carve(glm::vec3 pos);
  // Subtract the volume swept by the tool along the linear segment p0 -> p1 in one dispatch.
  void carveSwept(glm::vec3 p0, glm::vec3 p1);
  // Block until all queued GPU carving work has completed (for timing/sync).
  void finishGPU();

  // Set Voxelized Objects
  void setWorkpiece(std::string workpiecePath);
  void setTool(std::string toolPath);

  void setToolPosition(const glm::vec3& pos);
  void setProjectionType(ProjectionType type) { projectionType = type; };

  // G-code unit handling (see GcodeUnits). Defaults: VOXEL, zero work offset.
  void setGcodeUnits(GcodeUnits u) { gcodeUnits = u; }
  void setWorkOffsetMm(const glm::vec3& mm) { workOffsetMm = mm; }
  // Validate the stock/tool pairing for the active units mode. In MM mode the
  // voxel sizes MUST match (the integer GPU kernels add tool transitions straight
  // into stock indices); returns false with an explanatory error if they differ.
  // In VOXEL mode it only warns on a mismatch, since that mode is grid-index only.
  // Call after setWorkpiece()+setTool(), before carving.
  bool checkUnitsConsistency() const;

  void copyBack() {
    // Copy back the voxelized workpiece data after carving
    ops.subtractGPU_copyback(ops.getObjects()[0]);  // Copy back from GPU to CPU
  }
  VoxelObject getWorkpiece() const {
    if (ops.getObjects().empty()) {
      throw std::runtime_error("No workpiece voxel object loaded.");
    }
    return ops.getObjects()[0];  // Return the first object (workpiece)
  }

  // Save the carved workpiece (object 0) to a .bin voxel file.
  // Call after copyBack() so the CPU-side data is populated.
  bool saveWorkpiece(const std::string& path) { return ops.save(path, 0); }

 private:
  void init();
  bool shouldClose() const;
  void printCounter(int value);

  Shader* shader = nullptr;
  // std::unique_ptr<Shader> shader;
  Shader* shader_flat = nullptr;
  Shader* shader_raymarching = nullptr;

  GLFWwindow* window = nullptr;
  glm::vec3 toolPosition;
  glm::mat4 projection, view;
  ProjectionType projectionType = ProjectionType::ORTHOGRAPHIC;  // Default to orthographic projection

  // G-code unit interpretation. VOXEL (default) reproduces the legacy behaviour
  // where a G-code number is a stock voxel index; MM treats it as world mm.
  GcodeUnits gcodeUnits = GcodeUnits::VOXEL;
  glm::vec3 workOffsetMm = glm::vec3(0.0f);  // G-code origin offset from the stock centre, in mm (MM mode only)

  void checkContext();
  void createShaders();
  void setMouseCallbacks();

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
  // BoolOps* ops = nullptr;  // Boolean operations for voxel objects
  BoolOps ops;

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

  long carvingCounter = 0;  // Counter for carving operations
};
