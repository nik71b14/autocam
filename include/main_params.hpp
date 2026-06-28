#pragma once

#include <glm/glm.hpp>

// =============================================================================
//  main_params.hpp - Default parameters for the autocam sub-commands.
//
//  These are the FALLBACK values used when the corresponding command-line
//  option is omitted (see DOCS/MANUAL.md). This is the single place to change
//  the defaults; nothing here is baked into the binary's behaviour anymore.
// =============================================================================

// --- Default input paths ----------------------------------------------------
#define GCODE_PATH "gcode/square_600.gcode"                    // simulate --gcode
#define DEFAULT_WORKPIECE_BIN "test/workpiece_100_100_50.bin"  // simulate --workpiece
#define DEFAULT_TOOL_BIN "test/hemispheric_mill_10.bin"        // simulate --tool

// --- Voxelization defaults --------------------------------------------------
#define RESOLUTION 0.1            // voxel size in object units, e.g. mm (voxelize --res)
#define DEFAULT_MEM_MB 512        // GPU memory budget in MB (voxelize --mem-mb)
#define WHITE glm::vec3(1.0f, 1.0f, 1.0f)
