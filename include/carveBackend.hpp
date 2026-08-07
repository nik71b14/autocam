#pragma once

#include <glad/glad.h>  // glFinish (FlatCarveBackend::finish)

#include <glm/glm.hpp>

#include "boolOps.hpp"  // BoolOps, VoxelObject

// =============================================================================
//  ICarveBackend — the seam between the G-code driver and the GPU carver, so a
//  SECOND carving algorithm can be swapped in at RUNTIME for A/B comparison
//  (env AUTOCAM_CARVE_BACKEND=flat|sparse) without recompiling or using git.
//
//  Both backends must produce BIT-EXACT output (identical geometry). The `flat`
//  backend is a thin adapter over the existing, unchanged BoolOps path; `sparse`
//  is the tiled backend (see DOCS/DEV_PLAN/sparse-tile-carve-plan.md).
// =============================================================================
class ICarveBackend {
 public:
  virtual ~ICarveBackend() = default;

  // Short backend name ("flat", "sparse"), for logging / benchmarks.
  virtual const char* name() const = 0;

  // Make the stock (object to carve) and tool resident in this backend's layout.
  // Called once after both are loaded, before any carveSwept().
  virtual void prepare(const VoxelObject& stock, const VoxelObject& tool) = 0;

  // Subtract the volume swept by the tool along one linear toolpath segment.
  // `startOffset` is the tool-centre offset (stock voxels) at the segment start,
  // `displacement` the offset to its end; `segmentIndex >= 0` enables removed-voxel
  // accounting (fitness), -1 disables it. Returns false on a hard error.
  virtual bool carveSwept(const glm::ivec3& startOffset, const glm::ivec3& displacement, int segmentIndex) = 0;

  // Block until queued GPU carving has completed (timing/sync).
  virtual void finish() = 0;

  // Assemble the carved stock into a compact VoxelObject (row-major columns) —
  // byte-identical to the flat backend's copyBack output.
  virtual void readResult(VoxelObject& out) = 0;
};

// The current production backend: the flat fixed-stride GPU carver (BoolOps).
// A thin adapter that forwards to the existing, unchanged code path, so selecting
// "flat" is byte-for-byte identical to before this abstraction existed.
class FlatCarveBackend : public ICarveBackend {
 public:
  explicit FlatCarveBackend(BoolOps& ops) : ops_(ops) {}
  const char* name() const override { return "flat"; }

  void prepare(const VoxelObject&, const VoxelObject&) override {
    ops_.subtractGPU_init(ops_.getObjects()[0], ops_.getObjects()[1]);  // uses the objects already loaded in ops
  }
  bool carveSwept(const glm::ivec3& s, const glm::ivec3& d, int seg) override { return ops_.subtractSwept(s, d, seg); }
  void finish() override { glFinish(); }
  void readResult(VoxelObject& out) override {
    ops_.subtractGPU_copyback(ops_.getObjects()[0]);  // GPU compaction + readback into object 0
    out = ops_.getObjects()[0];
  }

 private:
  BoolOps& ops_;
};
