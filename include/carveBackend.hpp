#pragma once

#include <glm/glm.hpp>

#include "boolOps.hpp"  // BoolOps, VoxelObject

// =============================================================================
//  ICarveBackend — the seam between the G-code driver and the GPU carver, so a
//  SECOND carving algorithm can be swapped in at RUNTIME for A/B comparison
//  (env AUTOCAM_CARVE_BACKEND=flat|sparse) without recompiling or using git.
//
//  Stage 0 (scaffolding) abstracts only the per-segment swept subtraction — the
//  one operation that differs between backends — and provides the `flat` backend
//  as a thin adapter over the existing, unchanged BoolOps path. Later stages grow
//  this interface (stock residency, result assembly, removed-voxel tracking) as
//  the sparse tiled backend needs them.  See DOCS/DEV_PLAN/sparse-tile-carve-plan.md.
// =============================================================================
class ICarveBackend {
 public:
  virtual ~ICarveBackend() = default;

  // Short name of the backend ("flat", "sparse"), for logging / benchmarks.
  virtual const char* name() const = 0;

  // Subtract the volume swept by the tool along one linear toolpath segment.
  // `startOffset` is the tool-centre offset (stock voxels) at the segment start,
  // `displacement` the offset to its end. `segmentIndex >= 0` enables this
  // segment's removed-voxel accounting (fitness); -1 disables it. Returns false
  // on a hard error. Must be BIT-EXACT across backends (identical geometry).
  virtual bool carveSwept(const glm::ivec3& startOffset, const glm::ivec3& displacement, int segmentIndex) = 0;
};

// The current production backend: the flat fixed-stride GPU carver (BoolOps).
// A thin adapter that forwards to the existing, unchanged code path, so selecting
// "flat" is byte-for-byte identical to before this abstraction existed.
class FlatCarveBackend : public ICarveBackend {
 public:
  explicit FlatCarveBackend(BoolOps& ops) : ops_(ops) {}
  const char* name() const override { return "flat"; }
  bool carveSwept(const glm::ivec3& s, const glm::ivec3& d, int seg) override { return ops_.subtractSwept(s, d, seg); }

 private:
  BoolOps& ops_;
};
