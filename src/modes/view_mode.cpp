// =============================================================================
//  view_mode.cpp - `view` sub-command.
//
//  Loads a .bin voxel object and shows it with the raymarching VoxelViewer.
//  Replaces the former VOXEL_VIEWER_TESTING #ifdef block.
//
//  Usage:
//    voxelize view <file.bin> [--ortho]
// =============================================================================

#include <iostream>
#include <string>

#include "GLUtils.hpp"
#include "boolOps.hpp"  // BoolOps + VoxelObject
#include "cli.hpp"
#include "meshDisplay.hpp"
#include "modes.hpp"
#include "voxelViewer.hpp"

int runView(const CliArgs& args) {
  if (args.positionals.empty()) {
    std::cerr << "view: missing input .bin path.\n";
    printUsage();
    return EXIT_FAILURE;
  }
  const std::string binPath = args.positionals[0];
  const bool ortho = args.has("--ortho");

  // BoolOps requires an active OpenGL context (its constructor throws without
  // one), so create a hidden loader context before loading the .bin.
  GLFWwindow* window = nullptr;
  setupGLContext(&window, 1, 1, "autocam - view (loader)", true);

  // Load inside a scope so BoolOps (which owns GL resources) is destroyed while
  // the loader context is still current — BEFORE destroyGLContext().
  VoxelObject obj;
  bool ok = false;
  {
    BoolOps ops;
    ok = ops.load(binPath);
    if (ok) obj = ops.getObjects()[0];
  }
  destroyGLContext(window);

  if (!ok) {
    std::cerr << "Failed to load voxel object: " << binPath << "\n";
    return EXIT_FAILURE;
  }

  // --mesh / --out-mesh: show (and/or save) a marching-cubes mesh instead of the
  // raymarcher. --out-mesh alone works headless (no window). --mesh-step subsamples.
  const std::string outMesh = args.get("--out-mesh", "");
  if (args.has("--mesh") || !outMesh.empty()) {
    const int meshStep = args.getInt("--mesh-step", 1);
    return showVoxelObjectAsMesh(obj, meshStep, outMesh, args.has("--mesh")) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  // VoxelViewer manages its own OpenGL context/window for the render loop.
  VoxelViewer viewer(obj.compressedData, obj.prefixSumData, obj.params);
  viewer.setOrthographic(ortho);
  viewer.run();
  return EXIT_SUCCESS;
}
