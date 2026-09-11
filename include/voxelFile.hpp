#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "voxelizer.hpp"  // VoxelizationParams

// =============================================================================
//  voxelFile — the ONE place that knows the on-disk layout of a voxel object.
//
//  Previously the .bin layout was open-coded (raw struct dump, no header) in three
//  spots — Voxelizer::save, BoolOps::save and BoolOps::load — so a single struct
//  reorder would silently corrupt every file. This centralises it and makes the
//  format self-describing:
//
//      ['A','V','O','X'][u32 version][u32 paramsBytes][VoxelizationParams]
//      [u64 dataBytes][u64 prefixBytes][compressed u32...][prefixSum u32...]
//
//  A headerless/old or corrupt file is REJECTED with a clear message rather than
//  mis-parsed. Artifacts (workpiece/tool .bin) are regenerated on demand with
//  `autocam voxelize`, so there is intentionally no legacy (headerless) reader.
// =============================================================================
namespace voxelfile {

constexpr char MAGIC[4] = {'A', 'V', 'O', 'X'};
constexpr uint32_t VERSION = 1;

// Write params + the two transition buffers to `path`. Returns false (with an
// explanatory message) on any error. `compressed`/`prefix` use unsigned int,
// which is what GLuint is — std::vector<GLuint> binds here directly.
inline bool write(const std::string& path, const VoxelizationParams& params, const std::vector<unsigned int>& compressed,
                  const std::vector<unsigned int>& prefix) {
  if (compressed.empty() || prefix.empty()) {
    std::cerr << "voxelfile::write: no data to save (run voxelization first)." << std::endl;
    return false;
  }
  std::ofstream f(path, std::ios::binary);
  if (!f) {
    std::cerr << "voxelfile::write: cannot open for writing: " << path << std::endl;
    return false;
  }

  const uint32_t version = VERSION;
  const uint32_t paramsBytes = static_cast<uint32_t>(sizeof(VoxelizationParams));
  const uint64_t dataBytes = static_cast<uint64_t>(compressed.size()) * sizeof(unsigned int);
  const uint64_t prefixBytes = static_cast<uint64_t>(prefix.size()) * sizeof(unsigned int);

  f.write(MAGIC, 4);
  f.write(reinterpret_cast<const char*>(&version), sizeof(uint32_t));
  f.write(reinterpret_cast<const char*>(&paramsBytes), sizeof(uint32_t));
  f.write(reinterpret_cast<const char*>(&params), paramsBytes);
  f.write(reinterpret_cast<const char*>(&dataBytes), sizeof(uint64_t));
  f.write(reinterpret_cast<const char*>(&prefixBytes), sizeof(uint64_t));
  f.write(reinterpret_cast<const char*>(compressed.data()), static_cast<std::streamsize>(dataBytes));
  f.write(reinterpret_cast<const char*>(prefix.data()), static_cast<std::streamsize>(prefixBytes));

  if (!f) {
    std::cerr << "voxelfile::write: failed while writing: " << path << std::endl;
    return false;
  }
  return true;
}

// Read params + the two transition buffers from `path`. Returns false (with an
// explanatory message) on a missing file, bad magic, unsupported version, or
// truncation. Forward-compatible: if a future file carries a larger params block,
// the overlap is copied and the extra bytes are skipped.
inline bool read(const std::string& path, VoxelizationParams& params, std::vector<unsigned int>& compressed,
                 std::vector<unsigned int>& prefix) {
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    std::cerr << "voxelfile::read: cannot open for reading: " << path << std::endl;
    return false;
  }

  char magic[4] = {0, 0, 0, 0};
  uint32_t version = 0;
  uint32_t paramsBytes = 0;
  f.read(magic, 4);
  f.read(reinterpret_cast<char*>(&version), sizeof(uint32_t));
  f.read(reinterpret_cast<char*>(&paramsBytes), sizeof(uint32_t));
  if (!f || std::memcmp(magic, MAGIC, 4) != 0) {
    std::cerr << "voxelfile::read: '" << path << "' is not an autocam voxel file (bad magic).\n"
              << "  It may be an old headerless .bin — regenerate it with `autocam voxelize`." << std::endl;
    return false;
  }
  if (version != VERSION) {
    std::cerr << "voxelfile::read: '" << path << "' has unsupported version " << version << " (expected " << VERSION
              << "). Regenerate it with `autocam voxelize`." << std::endl;
    return false;
  }

  // Copy the overlapping prefix of the params block; skip any extra future bytes.
  params = VoxelizationParams{};
  const uint32_t selfBytes = static_cast<uint32_t>(sizeof(VoxelizationParams));
  const uint32_t copyBytes = paramsBytes < selfBytes ? paramsBytes : selfBytes;
  f.read(reinterpret_cast<char*>(&params), copyBytes);
  if (paramsBytes > copyBytes) f.seekg(paramsBytes - copyBytes, std::ios::cur);

  uint64_t dataBytes = 0;
  uint64_t prefixBytes = 0;
  f.read(reinterpret_cast<char*>(&dataBytes), sizeof(uint64_t));
  f.read(reinterpret_cast<char*>(&prefixBytes), sizeof(uint64_t));
  if (!f) {
    std::cerr << "voxelfile::read: truncated header in " << path << std::endl;
    return false;
  }

  compressed.resize(dataBytes / sizeof(unsigned int));
  prefix.resize(prefixBytes / sizeof(unsigned int));
  f.read(reinterpret_cast<char*>(compressed.data()), static_cast<std::streamsize>(dataBytes));
  f.read(reinterpret_cast<char*>(prefix.data()), static_cast<std::streamsize>(prefixBytes));
  if (!f) {
    std::cerr << "voxelfile::read: truncated data in " << path << std::endl;
    return false;
  }
  return true;
}

}  // namespace voxelfile
