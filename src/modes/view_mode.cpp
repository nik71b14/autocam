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

  BoolOps ops;
  if (!ops.load(binPath)) {
    std::cerr << "Failed to load voxel object: " << binPath << "\n";
    destroyGLContext(window);
    return EXIT_FAILURE;
  }
  const VoxelObject& obj = ops.getObjects()[0];

  // VoxelViewer creates its own window/context for the render loop.
  VoxelViewer viewer(obj.compressedData, obj.prefixSumData, obj.params);
  viewer.setOrthographic(ortho);
  viewer.run();

  destroyGLContext(window);
  return EXIT_SUCCESS;
}
