// =============================================================================
//  simulate_mode.cpp - `simulate` sub-command.
//
//  Loads a G-code toolpath, a voxelized workpiece and a voxelized tool, then
//  steps the tool along the path carving the workpiece on the GPU. Optionally
//  saves the carved result and/or shows it in the raymarching viewer.
//  Replaces the former GCODE_TESTING #ifdef block.
//
//  Usage:
//    voxelize simulate --gcode <f.gcode> --workpiece <w.bin> --tool <t.bin>
//                      [--out <r.bin>] [--step <float>] [--perspective] [--no-view]
// =============================================================================

#include <glm/glm.hpp>

#include <chrono>
#include <iostream>
#include <string>

#include "GLUtils.hpp"
#include "cli.hpp"
#include "gcode.hpp"
#include "gcodeViewer.hpp"  // GcodeViewer, ProjectionType (also pulls in VoxelObject)
#include "main_params.hpp"
#include "modes.hpp"
#include "voxelViewer.hpp"

int runSimulate(const CliArgs& args) {
  // Resolve inputs from CLI, falling back to the defaults in main_params.hpp.
  const std::string gcodePath = args.get("--gcode", GCODE_PATH);
  const std::string workpiecePath = args.get("--workpiece", DEFAULT_WORKPIECE_BIN);
  const std::string toolPath = args.get("--tool", DEFAULT_TOOL_BIN);
  const float step = args.getFloat("--step", 2.0f);
  const bool showViewer = !args.has("--no-view");
  // The simulation defaults to an orthographic (top-down CNC) view; --perspective switches it.
  const ProjectionType projection =
      args.has("--perspective") ? ProjectionType::PERSPECTIVE : ProjectionType::ORTHOGRAPHIC;

  // A visible OpenGL context/window is required for the carving pipeline.
  GLFWwindow* window = nullptr;
  setupGLContext(&window, 800, 600, "autocam - simulate", false);

  // Load and validate the G-code toolpath.
  GCodeInterpreter interpreter;
  interpreter.setVerbose(args.has("--verbose"));  // off by default; --verbose dumps each command
  if (!interpreter.loadFile(gcodePath)) {
    std::cerr << "Failed to load G-code file: " << gcodePath << "\n";
    destroyGLContext(window);
    return EXIT_FAILURE;
  }
  if (!interpreter.checkFile()) {
    std::cerr << "Invalid G-code file: " << gcodePath << "\n";
    destroyGLContext(window);
    return EXIT_FAILURE;
  }

  // Carve inside a scope so that GcodeViewer (which owns GL resources, including a
  // BoolOps member) is destroyed while the OpenGL context is still current — BEFORE
  // destroyGLContext()/glfwTerminate(). Otherwise its destructor's GL calls would
  // run with no live context and crash.
  VoxelObject carved;
  {
    GcodeViewer gCodeViewer(window, interpreter.getToolpath());
    gCodeViewer.setProjectionType(projection);
    gCodeViewer.setWorkpiece(workpiecePath);
    gCodeViewer.setTool(toolPath);

    // Step the tool along the toolpath, carving the workpiece each step.
    interpreter.beginJog();
    auto tStart = std::chrono::high_resolution_clock::now();
    long steps = 0;
    while (!interpreter.jogComplete()) {
      interpreter.jog(step);  // advance by `step` voxel units (TODO: real mm units)
      glm::vec3 pos = interpreter.getCurrentPosition();
      gCodeViewer.carve(pos);
      ++steps;
    }
    auto tEnqueued = std::chrono::high_resolution_clock::now();  // dispatches enqueued (GPU still working)
    interpreter.resetJog();

    // Pull the carved workpiece back from the GPU to CPU memory.
    // copyBack() forces GPU completion (sync) + readback + CPU recompression.
    gCodeViewer.copyBack();
    auto tDone = std::chrono::high_resolution_clock::now();

    const double enqueueMs = std::chrono::duration<double, std::milli>(tEnqueued - tStart).count();
    const double totalMs = std::chrono::duration<double, std::milli>(tDone - tStart).count();
    std::cout << "\n";  // terminate the in-place carving counter line
    std::cout << "Carving: " << steps << " passi | enqueue " << enqueueMs
              << " ms | totale (incl. GPU sync + copyback) " << totalMs << " ms\n";

    // Optionally persist the carved result (reuses BoolOps' .bin format).
    if (args.has("--out")) {
      const std::string outPath = args.get("--out", "");
      if (gCodeViewer.saveWorkpiece(outPath))
        std::cout << "Saved carved workpiece -> " << outPath << "\n";
      else
        std::cerr << "Failed to save carved workpiece to: " << outPath << "\n";
    }

    if (showViewer) carved = gCodeViewer.getWorkpiece();

    // TODO: Marching Cubes mesh extraction of the carved result is disabled here
    // (a face at the extreme X value does not generate a corresponding mesh face).
    // Kept as a future feature; see marchingCubes.{hpp,cpp} and MeshViewer.
  }  // gCodeViewer destroyed here, while the context is still current

  destroyGLContext(window);

  if (showViewer) {
    // VoxelViewer manages its own OpenGL context/window (re-inits GLFW).
    VoxelViewer viewer(carved.compressedData, carved.prefixSumData, carved.params);
    viewer.run();
  }

  return EXIT_SUCCESS;
}
