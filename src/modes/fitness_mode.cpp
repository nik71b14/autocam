// =============================================================================
//  fitness_mode.cpp - `fitness` sub-command.
//
//  Headless evaluator for the genetic-algorithm toolpath generator. Given a
//  workpiece (stock), a tool, a G-code gene and the voxelized TARGET part, it
//  carves the stock with the gene on the GPU and scores the result against the
//  target, producing raw metrics plus a single scalar fitness (LOWER is better).
//
//  Objectives (see DOCS/MANUAL.md "Fitness evaluator"):
//    * Accuracy  - GOUGE (part present, material removed = irreversible) dominates;
//                  EXCESS (leftover material where the part is empty) is minimised.
//    * Time      - estimated cycle time (feed moves + rapid air moves + corner decel).
//    * Safety    - peak engagement (removed voxels per voxel of advance) vs e_break.
//    * Smoothness- frequency of abrupt direction changes on cutting moves.
//
//  Usage:
//    autocam fitness --gcode <f.gcode> --workpiece <w.bin> --tool <t.bin>
//                    --target <part.bin> [--config <fitness.conf>]
//                    [--gcode-units mm|voxel] [--work-origin x,y,z]
// =============================================================================

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "GLUtils.hpp"
#include "cli.hpp"
#include "coordinateSystem.hpp"
#include "fitnessConfig.hpp"
#include "gcode.hpp"
#include "gcodeViewer.hpp"  // GcodeViewer, GcodeUnits, VoxelObject
#include "main_params.hpp"
#include "modes.hpp"
#include "voxelFile.hpp"

namespace {

constexpr double kPi = 3.14159265358979323846;

// Transitions of one column of a voxel object (sorted ascending Z, parity toggles).
struct Column {
  const uint32_t* z = nullptr;
  int n = 0;
};

Column columnOf(const VoxelObject& obj, int x, int y) {
  const int resX = obj.params.resolutionXYZ.x;
  const long index = static_cast<long>(y) * resX + x;
  const uint32_t start = obj.prefixSumData[index];
  // prefixSumData has exactly resX*resY entries (no trailing sentinel): the last
  // column's end is the total transition count, not prefixSumData[index+1].
  const uint32_t end = (static_cast<size_t>(index) + 1 < obj.prefixSumData.size())
                           ? obj.prefixSumData[index + 1]
                           : static_cast<uint32_t>(obj.compressedData.size());
  Column c;
  c.z = (end > start) ? &obj.compressedData[start] : nullptr;
  c.n = static_cast<int>(end - start);
  return c;
}

// Compare a carved column against a target column over z in [0, zMax), where the
// target transitions are shifted by zShift into the carved (stock) z frame. Uses
// the parity convention of the occupancy read-back: voxel z is "inside" iff an odd
// number of transitions are <= z. Accumulates:
//   excess = carved-inside & target-outside   (leftover material, recoverable)
//   gouge  = carved-outside & target-inside    (missing material, irreversible)
//   carvedSolid / targetSolid = inside counts within the stock domain (for reporting).
void columnDiff(const Column& c, const Column& t, int zShift, int zMax, long& excess, long& gouge, long& carvedSolid,
                long& targetSolid) {
  int ci = 0, ti = 0;
  bool cIn = false, tIn = false;
  int z = 0;
  while (z < zMax) {
    while (ci < c.n && static_cast<int>(c.z[ci]) <= z) {
      cIn = !cIn;
      ++ci;
    }
    while (ti < t.n && (static_cast<int>(t.z[ti]) - zShift) <= z) {
      tIn = !tIn;
      ++ti;
    }
    int znext = zMax;
    if (ci < c.n && static_cast<int>(c.z[ci]) < znext) znext = static_cast<int>(c.z[ci]);
    if (ti < t.n) {
      int tv = static_cast<int>(t.z[ti]) - zShift;
      if (tv < znext) znext = tv;
    }
    if (znext <= z) znext = z + 1;  // remaining transitions are > z; guard against degenerate step
    if (znext > zMax) znext = zMax;
    const int len = znext - z;
    if (cIn && tIn) {
      carvedSolid += len;
      targetSolid += len;
    } else if (cIn) {
      carvedSolid += len;
      excess += len;
    } else if (tIn) {
      targetSolid += len;
      gouge += len;
    }
    z = znext;
  }
}

}  // namespace

int runFitness(const CliArgs& args) {
  // ---- Inputs -------------------------------------------------------------
  const std::string gcodePath = args.get("--gcode", GCODE_PATH);
  const std::string workpiecePath = args.get("--workpiece", DEFAULT_WORKPIECE_BIN);
  const std::string toolPath = args.get("--tool", DEFAULT_TOOL_BIN);
  const std::string targetPath = args.get("--target", "");
  const std::string configPath = args.get("--config", "");

  if (targetPath.empty()) {
    std::cerr << "fitness: --target <part.bin> is required (the voxelized finished part to match).\n";
    return EXIT_FAILURE;
  }

  FitnessConfig cfg;
  if (!loadFitnessConfig(configPath.empty() ? "fitness.conf" : configPath, cfg, /*required=*/!configPath.empty()))
    return EXIT_FAILURE;

  // G-code unit interpretation (same rules as `simulate`).
  const std::string unitsStr = args.get("--gcode-units", "voxel");
  GcodeUnits gcodeUnits;
  if (unitsStr == "voxel") {
    gcodeUnits = GcodeUnits::VOXEL;
  } else if (unitsStr == "mm") {
    gcodeUnits = GcodeUnits::MM;
  } else {
    std::cerr << "fitness: invalid --gcode-units '" << unitsStr << "' (expected 'mm' or 'voxel').\n";
    return EXIT_FAILURE;
  }
  glm::vec3 workOriginMm(0.0f);
  if (args.has("--work-origin")) {
    std::string s = args.get("--work-origin", "0,0,0");
    std::replace(s.begin(), s.end(), ',', ' ');
    std::istringstream iss(s);
    if (!(iss >> workOriginMm.x >> workOriginMm.y >> workOriginMm.z)) {
      std::cerr << "fitness: invalid --work-origin '" << args.get("--work-origin", "") << "' (expected x,y,z in mm).\n";
      return EXIT_FAILURE;
    }
  }

  // ---- Load the target part (voxelized) BEFORE touching GL -----------------
  VoxelObject target;
  if (!voxelfile::read(targetPath, target.params, target.compressedData, target.prefixSumData)) {
    std::cerr << "fitness: failed to load target '" << targetPath << "'.\n";
    return EXIT_FAILURE;
  }

  // ---- Load and validate the gene -----------------------------------------
  GLFWwindow* window = nullptr;
  setupGLContext(&window, 800, 600, "autocam - fitness", /*hideWindow=*/true);

  GCodeInterpreter interpreter;
  interpreter.setVerbose(args.has("--verbose"));
  if (!interpreter.loadFile(gcodePath) || !interpreter.checkFile()) {
    std::cerr << "fitness: failed to load/validate G-code '" << gcodePath << "'.\n";
    destroyGLContext(window);
    return EXIT_FAILURE;
  }

  VoxelObject carved;
  std::vector<GLuint> removed;  // voxels removed per segment
  std::vector<GcodePoint> toolpath;
  bool ok = true;
  {
    toolpath = interpreter.getToolpath();
    const int nSeg = static_cast<int>(toolpath.size()) - 1;

    GcodeViewer viewer(window, toolpath);
    viewer.setWorkpiece(workpiecePath);
    viewer.setTool(toolPath);
    viewer.setGcodeUnits(gcodeUnits);
    viewer.setWorkOffsetMm(workOriginMm);
    ok = viewer.checkUnitsConsistency();

    if (ok && nSeg > 0) {
      viewer.beginRemovedTracking(nSeg);
      for (int i = 0; i < nSeg; ++i) viewer.carveSwept(toolpath[i].position, toolpath[i + 1].position, i);
      viewer.finishGPU();
      removed = viewer.readRemovedPerSegment();
      viewer.copyBack();
      carved = viewer.getWorkpiece();
    } else if (ok) {
      std::cerr << "fitness: gene has no motion segments.\n";
      ok = false;
    }
  }  // viewer destroyed while the GL context is still current

  destroyGLContext(window);
  if (!ok) return EXIT_FAILURE;

  // ---- Accuracy: excess / gouge set-diff (carved vs target) ----------------
  const CoordinateSystem stock = CoordinateSystem::fromParams(carved.params);
  const CoordinateSystem tgt = CoordinateSystem::fromParams(target.params);
  if (!stock.sameVoxelSizeAs(tgt)) {
    std::cerr << "fitness: target voxel size " << tgt.voxelSizeMm.x << " mm != stock " << stock.voxelSizeMm.x
              << " mm.\n       Re-voxelize the target at the stock resolution (autocam voxelize <part.stl> --res "
              << stock.voxelSizeMm.x << ").\n";
    return EXIT_FAILURE;
  }
  // Integer voxel offset that aligns the target grid onto the stock grid in world mm.
  // targetIndex = stockIndex + delta; the two grids must be voxel-aligned (near-integer delta).
  const glm::vec3 delta = (stock.originMm() - tgt.originMm()) / stock.voxelSizeMm;
  const glm::vec3 offF = glm::round(delta);
  const glm::vec3 resid = glm::abs(delta - offF);
  if (std::max({resid.x, resid.y, resid.z}) > 0.25f) {
    std::cerr << "fitness: target grid is not voxel-aligned with the stock (residual "
              << std::max({resid.x, resid.y, resid.z})
              << " voxels).\n       Voxelize the target on the same grid as the stock (same --res and the same"
                 " bounding volume/center).\n";
    return EXIT_FAILURE;
  }
  const glm::ivec3 offset(offF);

  const int resX = stock.resolutionXYZ.x, resY = stock.resolutionXYZ.y, resZ = stock.resolutionXYZ.z;
  const int tResX = tgt.resolutionXYZ.x, tResY = tgt.resolutionXYZ.y;
  long excess = 0, gouge = 0, carvedSolid = 0, targetSolid = 0;
  const Column emptyCol{};
  for (int y = 0; y < resY; ++y) {
    for (int x = 0; x < resX; ++x) {
      const Column c = columnOf(carved, x, y);
      const int tx = x + offset.x, ty = y + offset.y;
      const bool inTarget = (tx >= 0 && tx < tResX && ty >= 0 && ty < tResY);
      const Column t = inTarget ? columnOf(target, tx, ty) : emptyCol;
      columnDiff(c, t, offset.z, resZ, excess, gouge, carvedSolid, targetSolid);
    }
  }

  // ---- Path / time / safety / smoothness metrics ---------------------------
  const double voxelMm = stock.voxelSizeMm.x;  // mm per voxel (isotropic)
  const bool mmUnits = (gcodeUnits == GcodeUnits::MM);
  const double turnRad = cfg.turn_angle_deg * kPi / 180.0;
  const int nSeg = static_cast<int>(toolpath.size()) - 1;

  double totalLenMm = 0, cutLenMm = 0, airLenMm = 0;
  double moveTimeSec = 0, cornerTimeSec = 0;
  double engageMax = 0, engageOverload = 0;
  long abruptTurns = 0;
  glm::vec3 prevDir(0.0f);
  bool prevValid = false;
  long prevRemoved = 0;

  for (int i = 0; i < nSeg; ++i) {
    const glm::vec3 d = toolpath[i + 1].position - toolpath[i].position;
    const double lenUnits = glm::length(d);
    const double lenMm = mmUnits ? lenUnits : lenUnits * voxelMm;
    const double lenVox = mmUnits ? (lenUnits / voxelMm) : lenUnits;
    const bool rapid = toolpath[i + 1].rapid;
    const double feedUnits = toolpath[i + 1].feedRate;
    const double feedMm = mmUnits ? feedUnits : feedUnits * voxelMm;
    const long rem = (i < static_cast<int>(removed.size())) ? static_cast<long>(removed[i]) : 0;
    const bool cutting = (rem > 0);

    totalLenMm += lenMm;
    (cutting ? cutLenMm : airLenMm) += lenMm;

    // Safety: engagement = voxels removed per voxel of advance (~ chip cross-section).
    if (lenVox > 1e-6) {
      const double engage = static_cast<double>(rem) / lenVox;
      engageMax = std::max(engageMax, engage);
      engageOverload += std::max(0.0, engage - cfg.e_break);
    }

    // Time: feed for a cut, rapid for an air move; fall back to default_feed if no F.
    double fMm = rapid ? cfg.rapid_feed : (feedMm > 1e-6 ? feedMm : cfg.default_feed);
    if (fMm < 1e-6) fMm = cfg.default_feed;
    moveTimeSec += lenMm / (fMm / 60.0);

    // Corner deceleration + smoothness: angle between consecutive segment directions.
    if (lenUnits > 1e-9) {
      const glm::vec3 dir = d / static_cast<float>(lenUnits);
      if (prevValid) {
        const double c = std::max(-1.0, std::min(1.0, static_cast<double>(glm::dot(prevDir, dir))));
        const double angle = std::acos(c);
        cornerTimeSec += cfg.corner_decel_s * (angle / kPi);  // every vertex costs some deceleration time
        if (angle > turnRad && cutting && prevRemoved > 0) ++abruptTurns;  // sharp turn WHILE cutting hurts finish
      }
      prevDir = dir;
      prevValid = true;
    }
    prevRemoved = rem;
  }

  const double timeSec = moveTimeSec + cornerTimeSec;
  const double turnFreq = (cutLenMm > 1e-9) ? (static_cast<double>(abruptTurns) / cutLenMm) : 0.0;

  // ---- Scalar fitness (LOWER is better) ------------------------------------
  const double fitness = cfg.w_gouge * static_cast<double>(gouge) + cfg.w_excess * static_cast<double>(excess) +
                         cfg.w_time * timeSec + cfg.w_engage * engageOverload + cfg.w_turn * turnFreq +
                         cfg.w_air * airLenMm + cfg.w_moves * static_cast<double>(nSeg);

  const double voxMm3 = voxelMm * voxelMm * voxelMm;
  const double coverage = (targetSolid > 0) ? static_cast<double>(targetSolid - gouge) / static_cast<double>(targetSolid) : 1.0;

  // ---- Report (machine-parseable `key value`, one per line) ----------------
  std::cout << "fitness " << fitness << "\n";
  std::cout << "gouge_voxels " << gouge << "\n";
  std::cout << "excess_voxels " << excess << "\n";
  std::cout << "gouge_mm3 " << gouge * voxMm3 << "\n";
  std::cout << "excess_mm3 " << excess * voxMm3 << "\n";
  std::cout << "carved_voxels " << carvedSolid << "\n";
  std::cout << "target_voxels " << targetSolid << "\n";
  std::cout << "coverage " << coverage << "\n";
  std::cout << "path_len_mm " << totalLenMm << "\n";
  std::cout << "cut_len_mm " << cutLenMm << "\n";
  std::cout << "air_len_mm " << airLenMm << "\n";
  std::cout << "est_time_s " << timeSec << "\n";
  std::cout << "move_time_s " << moveTimeSec << "\n";
  std::cout << "corner_time_s " << cornerTimeSec << "\n";
  std::cout << "engage_max " << engageMax << "\n";
  std::cout << "engage_overload " << engageOverload << "\n";
  std::cout << "abrupt_turns " << abruptTurns << "\n";
  std::cout << "turn_freq_per_mm " << turnFreq << "\n";
  std::cout << "n_moves " << nSeg << "\n";
  std::cout << "voxel_mm " << voxelMm << "\n";
  return EXIT_SUCCESS;
}
