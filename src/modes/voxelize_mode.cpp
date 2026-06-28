// =============================================================================
//  voxelize_mode.cpp - `voxelize` sub-command.
//
//  Loads an STL mesh, voxelizes it on the GPU and saves the result as a .bin
//  voxel object. Replaces the former VOXELIZATION_TESTING #ifdef block.
//
//  Usage:
//    voxelize voxelize <input.stl> [--out <file.bin>] [--res <float>] [--mem-mb <int>]
// =============================================================================

#include <glm/glm.hpp>
#include <iostream>
#include <string>

#include "cli.hpp"
#include "main_params.hpp"
#include "meshLoader.hpp"
#include "modes.hpp"
#include "utils.hpp"
#include "voxelizer.hpp"
#include "voxelizerUtils.hpp"

int runVoxelize(const CliArgs& args) {
  if (args.positionals.empty()) {
    std::cerr << "voxelize: missing input STL path.\n";
    printUsage();
    return EXIT_FAILURE;
  }
  const std::string input = args.positionals[0];

  // Build voxelization parameters from defaults (main_params.hpp) + CLI overrides.
  VoxelizationParams params;
  params.resolution = args.getFloat("--res", RESOLUTION);
  params.color = WHITE;
  params.maxMemoryBudgetBytes = static_cast<size_t>(args.getInt("--mem-mb", DEFAULT_MEM_MB)) * 1024 * 1024;
  params.slicesPerBlock = chooseOptimalPowerOfTwoSlicesPerBlock(params);

  // Default output: test/<stlname>.bin
  const std::string out = args.get("--out", "test/" + stlToBinName(getFileNameFromPath(input)));

  // Voxelizer::run() creates and owns its own OpenGL context internally.
  Mesh mesh = loadMesh(input.c_str());
  Voxelizer voxelizer(mesh, params);
  voxelizer.run();

  if (!voxelizer.save(out)) {
    std::cerr << "Failed to save voxel object to: " << out << "\n";
    return EXIT_FAILURE;
  }

  std::cout << "Voxelized '" << input << "' -> '" << out << "'\n";
  return EXIT_SUCCESS;
}
