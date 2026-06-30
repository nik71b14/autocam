#include "meshDisplay.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "GLUtils.hpp"
#include "marchingCubes.hpp"
#include "meshViewer.hpp"

bool showVoxelObjectAsMesh(const VoxelObject& obj, int meshStep, const std::string& outStl, bool interactive) {
  // A GL context must be current: MarchingCubes::go() calls glfwPollEvents() and
  // MeshViewer needs the context for its buffers/shaders. Create one window (hidden
  // when we are not displaying, e.g. headless STL export) and keep it alive for
  // both the mesher and the viewer.
  const int W = 1200;
  const int H = 800;
  GLFWwindow* window = nullptr;
  setupGLContext(&window, W, H, "autocam - mesh", /*hideWindow=*/!interactive);

  bool ok = true;
  {
    // Marching Cubes consumes the voxel object directly (same parity walk as the
    // raymarch shader); see marchingCubes.cpp.
    MarchingCubes mc(obj);
    mc.setStep(meshStep);
    mc.go();

    if (mc.getTriangles().empty()) {
      std::cerr << "Mesh is empty: no surface extracted (is the object empty?)." << std::endl;
      ok = false;
    } else {
      if (!outStl.empty()) mc.saveStl(outStl);

      if (interactive) {
        // MeshViewer owns GL resources, so keep it in this inner scope: it must be
        // destroyed while the context is still current, before destroyGLContext().
        MeshViewer viewer(window, W, H, mc.getVertices(), mc.getTriangles(), mc.getNormals());
        while (!glfwWindowShouldClose(window)) {
          glfwPollEvents();
          viewer.drawFrame();
        }
      }
    }
  }  // MeshViewer destroyed here, while the context is still current

  destroyGLContext(window);
  return ok;
}
