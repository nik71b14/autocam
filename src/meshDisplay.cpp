#include "meshDisplay.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "GLUtils.hpp"
#include "gpuMarchingCubes.hpp"
#include "marchingCubes.hpp"
#include "meshViewer.hpp"

// Run the mesh viewer's interactive loop with the existing CPU-buffer MeshViewer
// constructor (used by the CPU path and the GPU readback path).
static void runViewerLoop(GLFWwindow* window, int W, int H, const std::vector<float>& verts, const std::vector<int>& tris, const std::vector<float>& norms) {
  MeshViewer viewer(window, W, H, verts, tris, norms);
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    viewer.drawFrame();
  }
}

bool showVoxelObjectAsMesh(const VoxelObject& obj, int meshStep, int smoothIterations, const std::string& outStl, bool interactive, bool useGpu) {
  // A GL context must be current for both the (CPU or GPU) mesher and MeshViewer.
  // Create one window (hidden for headless STL export) and keep it alive for both.
  const int W = 1200;
  const int H = 800;
  GLFWwindow* window = nullptr;
  setupGLContext(&window, W, H, "autocam - mesh", /*hideWindow=*/!interactive);

  bool ok = true;
  {
    if (!useGpu) {
      // ---- CPU reference path (unchanged) ----
      MarchingCubes mc(obj);
      mc.setStep(meshStep);
      mc.go();
      mc.smooth(smoothIterations);  // weld + averaged normals + optional Taubin
      if (mc.getTriangles().empty()) {
        std::cerr << "Mesh is empty: no surface extracted (is the object empty?)." << std::endl;
        ok = false;
      } else {
        if (!outStl.empty()) mc.saveStl(outStl);
        if (interactive) runViewerLoop(window, W, H, mc.getVertices(), mc.getTriangles(), mc.getNormals());
      }
    } else {
      // ---- GPU path: edge-indexed Marching Cubes (smooth gradient normals) ----
      GpuMarchingCubes gmc(obj);
      gmc.setStep(meshStep);
      if (!gmc.run()) {
        ok = false;  // empty / failed (message printed by run())
      } else {
        // Geometric Taubin smoothing and STL export need the mesh on the CPU; read
        // it back into a MarchingCubes (reusing its smooth()/saveStl()). The pure
        // interactive view (smooth 0, no STL) draws the GPU buffers directly.
        // Streamed (large-grid) meshes live in CPU accumulators, not vbo/ebo, so they
        // also go through the readback (CPU MeshViewer) path.
        const bool needReadback = !outStl.empty() || smoothIterations > 0 || !gmc.gpuResident();
        if (needReadback) {
          MarchingCubes mc(obj);
          gmc.readbackTo(mc);
          if (smoothIterations > 0) mc.smooth(smoothIterations);
          if (!outStl.empty()) mc.saveStl(outStl);
          if (interactive) runViewerLoop(window, W, H, mc.getVertices(), mc.getTriangles(), mc.getNormals());
        } else if (interactive) {
          // Zero readback: bind the GPU vertex/index buffers straight into MeshViewer.
          MeshViewer viewer(window, W, H, gmc.vbo(), gmc.ebo(), gmc.indexCount(), gmc.bboxMin(), gmc.bboxMax());
          while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            viewer.drawFrame();
          }
        }
      }
    }
  }  // GpuMarchingCubes / MeshViewer destroyed here, while the context is still current

  destroyGLContext(window);
  return ok;
}
