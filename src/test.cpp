#include "test.hpp"

#include <iostream>
#include <string>

#include "boolOps.hpp"

void analizeVoxelizedObject(const std::string& filename) {
  BoolOps ops;
  if (!ops.load(filename)) {
    std::cerr << "Failed to load voxelized object." << std::endl;
    return;
  }
  const auto& obj = ops.getObjects().back();  // Get the last loaded object

  std::cout << "Voxelization Parameters:" << std::endl;
  std::cout << "  Resolution XYZ: (" << obj.params.resolutionXYZ.x << ", " << obj.params.resolutionXYZ.y << ", " << obj.params.resolutionXYZ.z << ")"
            << std::endl;
  std::cout << "  Max Transitions Per Z Column: " << obj.params.maxTransitionsPerZColumn << std::endl;
  std::cout << "  Z Span: " << obj.params.zSpan << std::endl;
  std::cout << "  Scale: " << obj.params.scale << std::endl;
  std::cout << "  Center: (" << obj.params.center.x << ", " << obj.params.center.y << ", " << obj.params.center.z << ")" << std::endl;
  std::cout << "  Color: (" << obj.params.color.x << ", " << obj.params.color.y << ", " << obj.params.color.z << ")" << std::endl;

  std::cout << "  Compressed Data Size: " << obj.compressedData.size() << " elements" << std::endl;
  std::cout << "  Prefix Sum Data Size: " << obj.prefixSumData.size() << " elements" << std::endl;
  std::cout << "  Total Compressed Data Size: " << obj.compressedData.size() * sizeof(GLuint) << " bytes" << std::endl;
  std::cout << "  Total Prefix Sum Data Size: " << obj.prefixSumData.size() * sizeof(GLuint) << " bytes" << std::endl;
  std::cout << "  Total Data Size: " << (obj.compressedData.size() + obj.prefixSumData.size()) * sizeof(GLuint) << " bytes" << std::endl;
  std::cout << "  Memory Usage: " << (obj.compressedData.size() + obj.prefixSumData.size()) * sizeof(GLuint) / (1024.0f * 1024.0f) << " MB" << std::endl;
  std::cout << "  Memory Usage (Compressed Data): " << obj.compressedData.size() * sizeof(GLuint) / (1024.0f * 1024.0f) << " MB" << std::endl;
  std::cout << "  Memory Usage (Prefix Sum Data): " << obj.prefixSumData.size() * sizeof(GLuint) / (1024.0f * 1024.0f) << " MB" << std::endl;

  const auto& res = obj.params.resolutionXYZ;
  size_t columnsWithMinZ = 0;
  size_t columnsWithMaxZ = 0;

  // Check for transitions at min Z (0) and max Z (res.z - 1)
  // Each column's data starts at obj.prefixSumData[colIdx]
  for (int x = 0; x < res.x; ++x) {
    for (int y = 0; y < res.y; ++y) {
      int colIdx = x * res.y + y;
      size_t start = obj.prefixSumData[colIdx];
      size_t end = obj.prefixSumData[colIdx + 1];
      bool hasMinZ = false, hasMaxZ = false;
      for (size_t i = start; i < end; ++i) {
        GLuint t = obj.compressedData[i];
        if (t == 0) hasMinZ = true;
        if (t == static_cast<GLuint>(res.z - 1)) hasMaxZ = true;
        if (hasMinZ && hasMaxZ) break;
      }
      if (hasMinZ) ++columnsWithMinZ;
      if (hasMaxZ) ++columnsWithMaxZ;
    }
  }

  std::cout << "Columns with transitions at min Z (0): " << columnsWithMinZ << std::endl;
  std::cout << "Columns with transitions at max Z (" << (res.z - 1) << "): " << columnsWithMaxZ << std::endl;

  return;
  
  // Modify the compressed data to remove max Z transitions,
  // then save the modified object appending "_mod" to the filename
  if (columnsWithMaxZ > 0) {
    std::vector<GLuint>& compressedData = const_cast<std::vector<GLuint>&>(obj.compressedData);
    for (int x = 0; x < res.x; ++x) {
      for (int y = 0; y < res.y; ++y) {
        int colIdx = x * res.y + y;
        size_t start = obj.prefixSumData[colIdx];
        size_t end = obj.prefixSumData[colIdx + 1];
        for (size_t i = start; i < end; ++i) {
          if (compressedData[i] == static_cast<GLuint>(res.z - 1)) {
            compressedData[i] = static_cast<GLuint>(res.z - 2);
          }
        }
      }
    }
    std::string outFilename = filename;
    size_t dot = outFilename.find_last_of('.');
    if (dot != std::string::npos) {
      outFilename.insert(dot, "_mod");
    } else {
      outFilename += "_mod";
    }
    if (ops.save(outFilename)) {
      std::cout << "Modified file saved as: " << outFilename << std::endl;
    } else {
      std::cerr << "Failed to save modified file." << std::endl;
    }
  }
}