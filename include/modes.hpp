#pragma once

// =============================================================================
//  modes.hpp - Entry points for the application's runtime sub-commands.
//
//  Each sub-command (previously selected at compile-time via #define in
//  main.cpp) is now a self-contained function chosen at runtime by main().
//  Every mode builds the parameters it needs from the CLI args (falling back
//  to the defaults in main_params.hpp) and returns an exit code.
// =============================================================================

#include "cli.hpp"

// voxelize: STL mesh -> voxelize -> save .bin voxel object.
int runVoxelize(const CliArgs& args);

// simulate: carve a workpiece along a G-code toolpath with a tool, then view.
int runSimulate(const CliArgs& args);

// view: load a .bin voxel object and show it with the raymarching viewer.
int runView(const CliArgs& args);

// Print top-level usage/help.
void printUsage();
