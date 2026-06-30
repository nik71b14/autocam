#pragma once

#include <string>

#include "boolOps.hpp"  // VoxelObject

// =============================================================================
//  meshDisplay - shared glue for the `--mesh` / `--out-mesh` path.
//
//  Turns a (carved or loaded) voxel object into a triangle mesh via Marching
//  Cubes and optionally saves it as a binary STL and/or shows it interactively
//  with MeshViewer. Centralises the window + GL-context + render-loop handling so
//  both `view` and `simulate` reuse the same, lifetime-correct code path.
// =============================================================================

// Generate a marching-cubes mesh from `obj` and:
//   - if `outStl` is non-empty, write it as a binary STL there;
//   - if `interactive` is true, open a window and display it until closed.
//
//   meshStep    subsampling factor (>= 1); higher = lighter / faster mesh.
//   outStl      STL output path ("" = don't save).
//   interactive open a viewer window (false for headless, e.g. with --no-view).
//
// Returns false if no surface could be extracted (empty mesh) or on a GL error.
bool showVoxelObjectAsMesh(const VoxelObject& obj, int meshStep, const std::string& outStl, bool interactive);
