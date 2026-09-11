#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "voxelizer.hpp"  // VoxelizationParams (the persisted record)

// =============================================================================
//  CoordinateSystem — the single source of truth for scale/units in autocam.
//
//  World space is MILLIMETRES. A voxel object is an integer grid `[0,resolutionXYZ)`
//  anchored in that world. Historically three coordinate spaces were conflated at
//  the boundaries (normalised `[-0.5,0.5]`, voxel-index, and original mm); this
//  value type centralises every conversion between them so the formulas live in
//  exactly ONE place instead of being duplicated as magic numbers across the
//  voxelizer, the GPU carver and the viewers.
//
//  It does NOT add persisted state: it is *constructed from* a VoxelizationParams
//  (`fromParams`). The persisted fields `resolution`, `resolutionXYZ` and `center`
//  are the canonical truth; `scale`/`zSpan` are derived quantities kept in the
//  record only for the voxelizer's internal slicing pass and the shader box.
// =============================================================================
struct CoordinateSystem {
  glm::vec3 voxelSizeMm = glm::vec3(1.0f);     // mm per voxel (isotropic today; vec3 future-proofs anisotropy)
  glm::ivec3 resolutionXYZ = glm::ivec3(1);    // grid size in voxels per axis
  glm::vec3 centerMm = glm::vec3(0.0f);        // world-mm position of the grid centre (= params.center)
  float zSpanNorm = 1.0f;                      // normalised Z extent the raymarch shader box uses (= params.zSpan)

  // Build the live transform from the persisted record. `resolution` is mm/voxel
  // (isotropic), `center` is the original bbox centre in mm, `zSpan` is the
  // normalised Z height produced at voxelization time.
  static CoordinateSystem fromParams(const VoxelizationParams& p) {
    CoordinateSystem cs;
    cs.voxelSizeMm = glm::vec3(p.resolution);
    cs.resolutionXYZ = p.resolutionXYZ;
    cs.centerMm = p.center;
    cs.zSpanNorm = p.zSpan;
    return cs;
  }

  // Physical size of the whole grid, in millimetres.
  glm::vec3 extentMm() const { return glm::vec3(resolutionXYZ) * voxelSizeMm; }

  // World-mm position of the corner of voxel (0,0,0): centre minus half the extent.
  glm::vec3 originMm() const { return centerMm - 0.5f * extentMm(); }

  // Integer voxel index of the grid centre (matches the `w/2,h/2,z/2` convention
  // used by the GPU subtract kernels).
  glm::ivec3 centerVoxel() const { return resolutionXYZ / 2; }

  // Convert an mm DISPLACEMENT (e.g. a tool-centre offset from the stock centre)
  // into an integer voxel displacement, snapping to the nearest voxel. This is the
  // unit-consistent replacement for treating raw G-code numbers as voxel indices.
  // Note: Z is returned un-inverted; the carving convention applies its own Z sign
  // (`z/2 - offset.z`), so callers keep that as-is.
  glm::ivec3 mmDisplacementToVoxels(const glm::vec3& dMm) const { return glm::ivec3(glm::round(dMm / voxelSizeMm)); }

  // Two objects can be carved against each other only if one of their voxels means
  // the same physical size — the integer GPU kernels add tool transitions directly
  // into stock indices. Compare voxel sizes within a relative epsilon.
  bool sameVoxelSizeAs(const CoordinateSystem& other, float relEps = 1e-3f) const {
    glm::vec3 d = glm::abs(voxelSizeMm - other.voxelSizeMm);
    glm::vec3 s = glm::max(glm::abs(voxelSizeMm), glm::abs(other.voxelSizeMm));
    return d.x <= relEps * s.x && d.y <= relEps * s.y && d.z <= relEps * s.z;
  }

  // Model matrix that maps the raymarch shader's FIXED local box
  // `[-0.5,0.5]^2 x [-zSpan/2,zSpan/2]` to the world-mm box. The per-axis scale
  // (extentMm / localExtent) means non-square footprints are NOT distorted — the
  // old `scale(1/params.scale)` hack only worked because it assumed a square XY
  // footprint. The shader keeps its local box unchanged; all physical sizing lives
  // here, so no GPU shader edit is needed to render at true size.
  glm::mat4 renderModelMatrix() const {
    const glm::vec3 localExtent(1.0f, 1.0f, zSpanNorm);  // size of the shader's local box
    const glm::vec3 scaleVec = extentMm() / localExtent;  // local -> mm, independently per axis
    glm::mat4 m(1.0f);
    m = glm::translate(m, centerMm);
    m = glm::scale(m, scaleVec);
    return m;
  }
};
