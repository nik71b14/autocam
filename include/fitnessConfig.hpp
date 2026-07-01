#pragma once

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// =============================================================================
//  FitnessConfig — tunable parameters for the `autocam fitness` evaluator.
//
//  The genetic-algorithm toolpath generator scores each candidate gene (a G-code
//  program) with a single scalar fitness (LOWER is better) plus a set of raw
//  metrics. The weights and thresholds below shape that score. They live in a
//  plain key=value text file (see fitness.conf) parsed with no third-party deps.
//
//  Objectives (see DOCS/MANUAL.md "Fitness evaluator" for the full rationale):
//    * Accuracy  — match the carved stock to the target part. GOUGE (part present
//                  but material removed) is near-irreversible => dominant penalty;
//                  EXCESS (leftover material where the part is empty) is recoverable.
//    * Time      — estimated cycle time; subsumes path length, air moves and, via
//                  corner deceleration, the cost of sharp direction changes.
//    * Safety    — peak material-removal per unit advance (engagement); above
//                  e_break the tool would break, so paths that exceed it are punished.
//    * Smoothness— FREQUENCY (per mm of cutting) of abrupt direction changes, which
//                  spoil the surface finish. A boustrophedon/spiral has a low
//                  frequency and is not penalised; a jittery path has a high one.
// =============================================================================
struct FitnessConfig {
  // --- Accuracy (dominant) -------------------------------------------------
  double w_gouge = 1000.0;   // penalty per gouged voxel (target present, material missing)
  double w_excess = 1.0;     // penalty per excess voxel (leftover material where part is empty)

  // --- Cycle time ----------------------------------------------------------
  double w_time = 1.0;         // penalty per second of estimated cycle time
  double rapid_feed = 5000.0;  // rapid (G0) speed, mm/min, used for air moves in the time model
  double default_feed = 400.0; // feed used when a cutting move carries no F, mm/min
  double corner_decel_s = 0.05;// seconds added per full 180 deg reversal (scaled by the turn angle)

  // --- Safety (tool breakage / wear) --------------------------------------
  double w_engage = 5.0;       // penalty per unit of engagement above e_break, summed over segments
  double e_break = 300.0;      // engagement threshold: removed voxels per voxel of advance => breakage risk

  // --- Smoothness (surface finish) ----------------------------------------
  double w_turn = 10.0;        // penalty per (abrupt turn / mm of cutting) — a frequency
  double turn_angle_deg = 60.0;// a direction change sharper than this counts as an abrupt turn

  // --- Parsimony (optional; 0 by default) ---------------------------------
  double w_air = 0.0;          // extra penalty per mm of air (non-cutting) travel; time already covers it
  double w_moves = 0.0;        // penalty per toolpath move (gene length)
};

// Parse a fitness config file (key = value, '#' comments, blank lines ignored).
// Unknown keys are reported but not fatal. Returns false only if the file exists
// but cannot be opened; a missing path leaves `cfg` at its defaults (returns true).
inline bool loadFitnessConfig(const std::string& path, FitnessConfig& cfg, bool required = false) {
  std::ifstream f(path);
  if (!f) {
    if (required) {
      std::cerr << "fitness: could not open config '" << path << "'\n";
      return false;
    }
    return true;  // no config => use defaults
  }

  auto trim = [](std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
  };

  std::string line;
  int lineNo = 0;
  while (std::getline(f, line)) {
    ++lineNo;
    // strip inline comments (# ...)
    size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    line = trim(line);
    if (line.empty()) continue;

    size_t eq = line.find('=');
    if (eq == std::string::npos) {
      std::cerr << "fitness config " << path << ":" << lineNo << ": missing '=' — skipped\n";
      continue;
    }
    std::string key = trim(line.substr(0, eq));
    std::string val = trim(line.substr(eq + 1));
    double v = 0.0;
    std::istringstream(val) >> v;

    if (key == "w_gouge") cfg.w_gouge = v;
    else if (key == "w_excess") cfg.w_excess = v;
    else if (key == "w_time") cfg.w_time = v;
    else if (key == "rapid_feed") cfg.rapid_feed = v;
    else if (key == "default_feed") cfg.default_feed = v;
    else if (key == "corner_decel_s") cfg.corner_decel_s = v;
    else if (key == "w_engage") cfg.w_engage = v;
    else if (key == "e_break") cfg.e_break = v;
    else if (key == "w_turn") cfg.w_turn = v;
    else if (key == "turn_angle_deg") cfg.turn_angle_deg = v;
    else if (key == "w_air") cfg.w_air = v;
    else if (key == "w_moves") cfg.w_moves = v;
    else std::cerr << "fitness config " << path << ":" << lineNo << ": unknown key '" << key << "' — ignored\n";
  }
  return true;
}
