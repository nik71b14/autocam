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

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
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
  const bool legacy = args.has("--legacy");  // per-step stamping (Phase 1) instead of swept (Phase 2)
  // The simulation defaults to an orthographic (top-down CNC) view; --perspective switches it.
  const ProjectionType projection =
      args.has("--perspective") ? ProjectionType::PERSPECTIVE : ProjectionType::ORTHOGRAPHIC;

  // How to interpret the G-code numbers. Default 'voxel' = legacy stock-voxel
  // indices (the original sample programs); 'mm' = world millimetres (canonical,
  // physically correct — needs the tool and stock voxelized at the same resolution).
  const std::string unitsStr = args.get("--gcode-units", "voxel");
  GcodeUnits gcodeUnits;
  if (unitsStr == "voxel") {
    gcodeUnits = GcodeUnits::VOXEL;
  } else if (unitsStr == "mm") {
    gcodeUnits = GcodeUnits::MM;
  } else {
    std::cerr << "Invalid --gcode-units '" << unitsStr << "' (expected 'mm' or 'voxel').\n";
    return EXIT_FAILURE;
  }

  // Optional G-code origin offset from the stock centre, in mm (MM mode only),
  // written as "--work-origin x,y,z". Default: origin at the stock centre.
  glm::vec3 workOriginMm(0.0f);
  if (args.has("--work-origin")) {
    std::string s = args.get("--work-origin", "0,0,0");
    std::replace(s.begin(), s.end(), ',', ' ');
    std::istringstream iss(s);
    if (!(iss >> workOriginMm.x >> workOriginMm.y >> workOriginMm.z)) {
      std::cerr << "Invalid --work-origin '" << args.get("--work-origin", "") << "' (expected x,y,z in mm).\n";
      return EXIT_FAILURE;
    }
  }

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
  bool ok = true;  // set false on a units/setup error so we can clean up GL before returning
  {
    // Extract the toolpath once (getToolpath re-parses the program, so don't call it twice).
    const std::vector<GcodePoint> toolpath = interpreter.getToolpath();

    GcodeViewer gCodeViewer(window, toolpath);
    gCodeViewer.setProjectionType(projection);
    gCodeViewer.setWorkpiece(workpiecePath);
    gCodeViewer.setTool(toolPath);
    gCodeViewer.setGcodeUnits(gcodeUnits);
    gCodeViewer.setWorkOffsetMm(workOriginMm);

    // Reject a stock/tool voxel-size mismatch in MM mode (only warns in VOXEL mode).
    ok = gCodeViewer.checkUnitsConsistency();
    if (ok) {

    auto tStart = std::chrono::high_resolution_clock::now();
    long steps = 0;
    if (legacy) {
      // Phase 1: stamp the full tool at every fixed jog step.
      interpreter.beginJog();
      while (!interpreter.jogComplete()) {
        interpreter.jog(step);  // advance by `step` voxel units (TODO: real mm units)
        glm::vec3 pos = interpreter.getCurrentPosition();
        gCodeViewer.carve(pos);
        ++steps;
      }
      interpreter.resetJog();
    } else {
      // Phase 2: one swept subtraction per linear toolpath segment.
      for (size_t i = 0; i + 1 < toolpath.size(); ++i) {
        gCodeViewer.carveSwept(toolpath[i].position, toolpath[i + 1].position);
        ++steps;
      }
    }
    // Wait for the GPU carving to actually complete, to measure the net carving time
    // separately from copyBack. This sync is free: copyBack() syncs anyway, so the
    // total is unchanged.
    gCodeViewer.finishGPU();
    auto tCarveDone = std::chrono::high_resolution_clock::now();

    // Pull the carved workpiece back from the GPU to CPU memory (readback + GPU compaction).
    gCodeViewer.copyBack();
    auto tDone = std::chrono::high_resolution_clock::now();

    const double carveMs = std::chrono::duration<double, std::milli>(tCarveDone - tStart).count();
    const double totalMs = std::chrono::duration<double, std::milli>(tDone - tStart).count();
    std::cout << "\n";  // terminate the in-place carving counter line
    std::cout << "Carving [" << (legacy ? "legacy" : "swept") << "]: " << steps
              << (legacy ? " passi" : " segmenti") << " | carving netto " << carveMs
              << " ms | totale (incl. copyback) " << totalMs << " ms\n";

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
    }  // end if (ok) — carving block ran only when the units/tool check passed
  }  // gCodeViewer destroyed here, while the context is still current

  destroyGLContext(window);

  if (!ok) return EXIT_FAILURE;  // units/setup error; GL already cleaned up above

  if (showViewer) {
    // VoxelViewer manages its own OpenGL context/window (re-inits GLFW).
    VoxelViewer viewer(carved.compressedData, carved.prefixSumData, carved.params);
    viewer.run();
  }

  return EXIT_SUCCESS;
}
