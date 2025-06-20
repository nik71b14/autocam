#include "test.hpp"

#include <chrono>
#include <glm/glm.hpp>
#include <iostream>
#include <string>

#include "boolOps.hpp"
#include "voxelViewer.hpp"

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

void subtract(const std::string& obj1Path, const std::string& obj2Path, glm::ivec3 offset) {
  BoolOps ops;
  if (!ops.load(obj1Path)) {
    std::cerr << "Failed to load first voxelized object." << std::endl;
    return;
  }
  if (!ops.load(obj2Path)) {
    std::cerr << "Failed to load second voxelized object." << std::endl;
    return;
  }

  auto& obj1 = ops.getObjects()[0];
  const auto& obj2 = ops.getObjects()[1];

  //@@@ Exection time measurement
  auto startTime = std::chrono::high_resolution_clock::now();

  long w1 = static_cast<long>(obj1.params.resolutionXYZ.x);
  long h1 = static_cast<long>(obj1.params.resolutionXYZ.y);
  long z1 = static_cast<long>(obj1.params.resolutionXYZ.z);
  long w2 = static_cast<long>(obj2.params.resolutionXYZ.x);
  long h2 = static_cast<long>(obj2.params.resolutionXYZ.y);
  long z2 = static_cast<long>(obj2.params.resolutionXYZ.z);

  long translateX = w1 / 2 + offset.x;
  long translateY = h1 / 2 + offset.y;
  long translateZ = z1 / 2 - offset.z;

  long minX = glm::clamp(translateX - w2 / 2, 0L, w1);
  long maxX = glm::clamp(translateX + w2 / 2, 0L, w1);
  long minY = glm::clamp(translateY - h2 / 2, 0L, h1);
  long maxY = glm::clamp(translateY + h2 / 2, 0L, h1);

  if (minX >= maxX || minY >= maxY) {
    std::cerr << "AOI is empty, no pixels to process." << std::endl;
    return;
  }

  std::vector<GLuint> compressedDataNew;
  std::vector<GLuint> prefixSumDataNew(obj1.prefixSumData.size(), 0);
  size_t currentOffset = 0;

  for (long y1 = 0; y1 < h1; ++y1) {
    for (long x1 = 0; x1 < w1; ++x1) {
      long idx1 = x1 + y1 * w1;

      if (x1 >= minX && x1 < maxX && y1 >= minY && y1 < maxY) {
        // AOI: perform subtraction

        // Extract packetZ1 for the current column in obj1
        size_t start1 = obj1.prefixSumData[idx1];
        size_t end1 = (static_cast<size_t>(idx1) + 1 < obj1.prefixSumData.size()) ? obj1.prefixSumData[idx1 + 1] : obj1.compressedData.size();
        std::vector<long> packetZ1(obj1.compressedData.begin() + start1, obj1.compressedData.begin() + end1);

        //%%%%%%
        if (packetZ1.empty()) {
          // No geometry in obj1 at this column → nothing to subtract → leave empty
          prefixSumDataNew[idx1] = currentOffset;
          continue;
        }

        // Extract packetZ2 for the current column in obj2
        long x2 = x1 - (translateX - w2 / 2);
        long y2 = y1 - (translateY - h2 / 2);
        long idx2 = x2 + y2 * w2;

        std::vector<long> packetZ2;

        // start2 and end2-1 are the indices in obj2's compressedData for the current column
        size_t start2 = obj2.prefixSumData[idx2];
        size_t end2 = (static_cast<size_t>(idx2) + 1 < obj2.prefixSumData.size()) ? obj2.prefixSumData[idx2 + 1] : obj2.compressedData.size();
        packetZ2.assign(obj2.compressedData.begin() + start2, obj2.compressedData.begin() + end2);

        //%%%%%%
        if (packetZ2.empty()) {
          // No transitions in obj2 for this column, copy packetZ1
          prefixSumDataNew[idx1] = currentOffset;
          compressedDataNew.insert(compressedDataNew.end(), packetZ1.begin(), packetZ1.end());
          currentOffset += packetZ1.size();
          continue;
        }

        for (auto& z : packetZ2) {
          z += translateZ - z2 / 2;  //%%%%% Note: z can be also outside the range of obj1, so we need to filter it later!!!
        }

        // Combine transitions
        std::vector<std::pair<long, int>> combined;
        for (auto z : packetZ1) combined.emplace_back(z, 0);
        for (auto z : packetZ2) combined.emplace_back(z, 1);
        std::sort(combined.begin(), combined.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first || (a.first == b.first && a.second > b.second); });

        // Initialize flags for transitions
        bool blackOn = false;
        bool redOn = false;
        std::vector<long> result;
        size_t i = 0;

        while (i < combined.size()) {
          long z = combined[i].first;
          bool hasBlack = false, hasRed = false;
          while (i < combined.size() && combined[i].first == z) {
            if (combined[i].second == 0) hasBlack = true;
            if (combined[i].second == 1) hasRed = true;
            ++i;
          }

          bool prevBlackOn = blackOn;
          bool prevRedOn = redOn;

          if (hasBlack) blackOn = !blackOn;
          if (hasRed) redOn = !redOn;

          if (hasBlack && hasRed) {
            bool blackWasOnToOff = prevBlackOn && !blackOn;
            bool blackWasOffToOn = !prevBlackOn && blackOn;
            bool redWasOnToOff = prevRedOn && !redOn;
            bool redWasOffToOn = !prevRedOn && redOn;

            if ((blackWasOffToOn && redWasOffToOn) || (blackWasOnToOff && redWasOnToOff)) {
              continue;  // discard
            } else {
              result.push_back(z);
            }
          } else if (hasBlack) {
            if (!prevBlackOn) {
              if (!prevRedOn) result.push_back(z);
            } else {
              if (!prevRedOn) result.push_back(z);
            }
          } else if (hasRed) {
            if (!prevRedOn) {
              if (prevBlackOn) result.push_back(z);
            } else {
              if (prevBlackOn) result.push_back(z);
            }
          }
        }

        //%%%%% Filter out-of-bounds
        std::vector<GLuint> filteredResult;
        for (auto z : result) {
          if (z >= 0 && z < z1) {
            filteredResult.push_back(z);
          }
        }

        prefixSumDataNew[idx1] = currentOffset;
        compressedDataNew.insert(compressedDataNew.end(), filteredResult.begin(), filteredResult.end());
        currentOffset += filteredResult.size();

      } else {
        // Non-AOI → copy existing data
        size_t start1 = obj1.prefixSumData[idx1];
        size_t end1 = (static_cast<size_t>(idx1) + 1 < obj1.prefixSumData.size()) ? obj1.prefixSumData[idx1 + 1] : obj1.compressedData.size();
        prefixSumDataNew[idx1] = currentOffset;
        compressedDataNew.insert(compressedDataNew.end(), obj1.compressedData.begin() + start1, obj1.compressedData.begin() + end1);
        currentOffset += (end1 - start1);
      }
    }
  }

  obj1.compressedData = std::move(compressedDataNew);
  obj1.prefixSumData = std::move(prefixSumDataNew);

  std::cout << "Subtraction completed. New compressed data size: " << obj1.compressedData.size() << std::endl;
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
  std::cout << "Execution time: " << duration << " ms" << std::endl;

  VoxelViewer viewer(obj1.compressedData, obj1.prefixSumData, obj1.params);
  viewer.setOrthographic(true);  // Set orthographic projection
  viewer.run();
}

void subtract_me(const std::string& obj1Path, const std::string& obj2Path, glm::ivec3 offset) {
  BoolOps ops;
  if (!ops.load(obj1Path)) {
    std::cerr << "Failed to load first voxelized object." << std::endl;
    return;
  }
  if (!ops.load(obj2Path)) {
    std::cerr << "Failed to load second voxelized object." << std::endl;
    return;
  }

  const auto& obj1 = ops.getObjects()[0];  // Get the last loaded object
  const auto& obj2 = ops.getObjects()[1];  // Get the second last loaded object

  // Each "obj" is a VoxelObject struct containing voxelization parameters and data.
  // It is supposed, at the moment that the real size of each voxel (obj1.params.scale) is the same for both objects
  // Each object has dimensions in voxels (obj.params.resolutionXYZ.x, obj.params.resolutionXYZ.y, obj.params.resolutionXYZ.z)
  // The reference object in whose coordinate system the subtraction is performed is obj1
  // The second object is obj2, which is subtracted from obj1
  // obj2 is offset by the given offset vector, whose value is in voxels
  // Each voxel object is represented by a VoxelObject struct, which contains the voxelization parameters and the data
  // The data is composed by the coordinate of a transition in the Z axis, i.e. a passage from empty to filled voxel or vice versa
  // The data is compressed, and the prefix sum data is used to access the data for each column in the voxel grid
  //   - The params are stored as object.params, which contains the resolution, scale, center, color, and other parameters
  //   - The compressed data is stored as object.compressedData, which is a vector of GLuints
  //   - The prefix sum data is stored as object.prefixSumData, which is a vector of GLuints
  // The obj are voxelized by "cutting" slices in the xy plane, at acertain Z coordinate, and storing the transitions in the compressed data
  // The dimensions of the slice are such that Xmax = Ymax (it is a square slice), and during the voxelization process the slices dimensions
  // are normalizet to a 1x1 square centered at the origin (0, 0) of the voxel grid
  // The dimension along the Z axis is free, and thus can be >, < or = to 1. The normalization factor is the same as that used
  // for the X and Y axes, i.e. the scale parameter in the voxelization parameters.
  //
  //    ______________
  //   |obj1          |
  //   |              |
  //   |              |
  //   |          ____|_
  //   |         |obj2| |
  //   |_________|____| |
  //             |______|
  //
  // The Area of Interest (AOI) for the subtraction is given by (obj1 AND obj2) in the XY plane, in the coordinate system of obj1.
  // Each pixel in the AOI can be identified by its (x, y) coordinates in the XY plane of obj1, so that its index in the prefix sum data is given by:
  // int idx1 = x + y * obj1.params.resolutionXYZ.y;
  // Now, i want a loop over the X,Y values of idx1 that belongs to the AOI, i.e. that for which (X >= offset.x && Y >= offset.y && belonging to obj1)
  //
  // In our test case, obj1 will be a 100x100x100 cube, obj2 will be a 50x50x50 cube

  long h1 = static_cast<long>(obj1.params.resolutionXYZ.y);
  long w1 = static_cast<long>(obj1.params.resolutionXYZ.x);
  long z1 = static_cast<long>(obj1.params.resolutionXYZ.z);
  std::cout << "obj1 X span: " << w1 << ", Y span: " << h1 << ", Z height" << z1 << std::endl;
  long h2 = static_cast<long>(obj2.params.resolutionXYZ.y);
  long w2 = static_cast<long>(obj2.params.resolutionXYZ.x);
  long z2 = static_cast<long>(obj2.params.resolutionXYZ.z);
  std::cout << "obj2 X span: " << w2 << ", Y span: " << h2 << ", Z height" << z2 << std::endl;

  long translateX = w1 / 2 + offset.x;
  long translateY = h1 / 2 + offset.y;
  long translateZ = z1 / 2 - offset.z;  //%%%%

  long minX = glm::clamp(translateX - w2 / 2, 0L, w1 - 1);
  long maxX = glm::clamp(translateX + w2 / 2, 0L, w1) - 1;
  long minY = glm::clamp(translateY - h2 / 2, 0L, h1 - 1);
  long maxY = glm::clamp(translateY + h2 / 2, 0L, h1 - 1);
  long minZ = glm::clamp(translateZ - z2 / 2, 0L, z1 - 1);
  long maxZ = glm::clamp(translateZ + z2 / 2, 0L, z1 - 1);

  long AOIWidth = maxX - minX;
  long AOIHeight = maxY - minY;
  long AOIHeightZ = maxZ - minZ;

  if (AOIWidth == 0 || AOIHeight == 0 || AOIHeightZ == 0) {
    std::cerr << "AOI is empty, no pixels to process." << std::endl;
    return;
  }

  std::cout << "AOI X: [" << minX << ", " << maxX << "), AOI Y: [" << minY << ", " << maxY << ")" << std::endl;
  std::cout << "AOI width: " << AOIWidth << ", AOI height: " << AOIHeight << std::endl;

  long x2, y2 = 0;

  y2 = 0;  // Reset y2 for the outer loop
  for (long y1 = minY; y1 < maxY; y1++) {
    x2 = 0;  // Reset x2 for each row
    for (long x1 = minX; x1 < maxX; x1++) {
      long idx1 = x1 + y1 * w1;
      // Here we would perform the subtraction logic
      // std::cout << "Subtracting at index: " << idx1 << " with offset: (" << offset.x << ", " << offset.y << ", " << offset.z << ")" << std::endl;
      size_t startIdx = obj1.prefixSumData[idx1];
      // The "packet" for column idx1 in obj1.compressedData spans from startIdx (inclusive) to endIdx (exclusive).
      // So, the last index belonging to the packet is actually endIdx - 1.
      size_t endIdx =
          (static_cast<size_t>(idx1 + 1) < obj1.prefixSumData.size()) ? obj1.prefixSumData[static_cast<size_t>(idx1 + 1)] : obj1.compressedData.size();
      // The valid range is [startIdx, endIdx), i.e., startIdx <= i < endIdx.
      // Now start1 and end1 define the range of compressedData for this column in obj1

      // Extract the packet of compressed data for this column in obj1
      std::vector<long> packetZ1(obj1.compressedData.begin() + startIdx, obj1.compressedData.begin() + endIdx);
      // Now 'packet' contains the compressed data for column (x, y) in obj1

      long idx2 = x2 + y2 * w2;
      size_t startIdx2 = obj2.prefixSumData[idx2];
      size_t endIdx2 =
          (static_cast<size_t>(idx2 + 1) < obj2.prefixSumData.size()) ? obj2.prefixSumData[static_cast<size_t>(idx2 + 1)] : obj2.compressedData.size();
      std::vector<long> packetZ2(obj2.compressedData.begin() + startIdx2, obj2.compressedData.begin() + endIdx2);

      // Move the values in packetZ2 to the coordinate system of obj1
      for (size_t i = 0; i < packetZ2.size(); ++i) {
        packetZ2[i] += translateZ - z2 / 2;                 // Shift the Z values by the offset.z
        packetZ2[i] = glm::clamp(packetZ2[i], 0L, h1 - 1);  // Clamp to valid Z range
      }

      // SUBTRACTION LOGIC ----------------------------------------------------
      // ✅ Sorting:
      //  First by increasing Z.
      //  When transitions coincide at the same Z, we handle red (obj2) transitions first, so subtraction happens cleanly.
      // ✅ Flags:
      //  blackOn: Are we inside a filled region of obj1?
      //  redOn: Are we inside a region to subtract (obj2)?
      // ✅ Conditions for Keeping a Transition:
      //  Red toggling on → if blackOn → keep (we're starting to subtract in filled region)
      //  Red toggling off → if blackOn → keep (we're ending subtraction in filled region)
      //  Black toggling on → if not redOn → keep (entering filled region, no subtraction here)
      //  Black toggling off → if not redOn → keep (exiting filled region, no subtraction here)
      // ✅ Coincident Transitions:
      //  Same type (both off→on or both on→off): discard → net effect zero.
      //  Mixed: merge → net subtraction boundary change → keep single transition.

      std::vector<std::pair<long, int>> combinedTransitions;
      for (const auto& z : packetZ1) {
        combinedTransitions.emplace_back(z, 0);  // black
      }
      for (const auto& z : packetZ2) {
        combinedTransitions.emplace_back(z, 1);  // red
      }

      // Sort by Z
      std::sort(combinedTransitions.begin(), combinedTransitions.end(), [](const auto& a, const auto& b) {
        return a.first < b.first || (a.first == b.first && a.second > b.second);
        // red before black on tie
      });

      std::vector<long> resultTransitions;
      bool blackOn = false;
      bool redOn = false;

      size_t i = 0;
      while (i < combinedTransitions.size()) {
        long z = combinedTransitions[i].first;

        // Collect all transitions at this z
        bool hasBlack = false;
        bool hasRed = false;

        while (i < combinedTransitions.size() && combinedTransitions[i].first == z) {
          if (combinedTransitions[i].second == 0) hasBlack = true;
          if (combinedTransitions[i].second == 1) hasRed = true;
          ++i;
        }

        // Save previous states
        bool prevBlackOn = blackOn;
        bool prevRedOn = redOn;

        // Toggle flags for those present at this z
        if (hasBlack) blackOn = !blackOn;
        if (hasRed) redOn = !redOn;

        if (hasBlack && hasRed) {
          // Both transitions at this Z
          bool blackWasOnToOff = prevBlackOn && !blackOn;
          bool blackWasOffToOn = !prevBlackOn && blackOn;
          bool redWasOnToOff = prevRedOn && !redOn;
          bool redWasOffToOn = !prevRedOn && redOn;

          if ((blackWasOffToOn && redWasOffToOn) || (blackWasOnToOff && redWasOnToOff)) {
            // Same type -> discard both
            continue;
          } else {
            // Mixed → keep one merged transition
            resultTransitions.push_back(z);
          }
        } else if (hasBlack) {
          // Only black transition at this z
          if (!prevBlackOn) {  // black off -> on
            if (!redOn) resultTransitions.push_back(z);
          } else {  // black on -> off
            if (!redOn) resultTransitions.push_back(z);
          }
        } else if (hasRed) {
          // Only red transition at this z
          if (!prevRedOn) {  // red off -> on
            if (blackOn) resultTransitions.push_back(z);
          } else {  // red on -> off
            if (blackOn) resultTransitions.push_back(z);
          }
        }
      }

      // END SUBTRACTION LOGIC ------------------------------------------------

      //@@@ DEBUG
      if (x2 == 0 && y2 == 0) {
        std::cout << "packetZ1: ";
        for (size_t i = 0; i < packetZ1.size(); ++i) {
          std::cout << packetZ1[i] << " ";
        }
        std::cout << std::endl;

        std::cout << "packetZ2: ";
        for (size_t i = 0; i < packetZ2.size(); ++i) {
          std::cout << packetZ2[i] << " ";
        }

        std::cout << "resultTransitions: ";
        for (size_t i = 0; i < resultTransitions.size(); ++i) {
          std::cout << resultTransitions[i] << " ";
        }
        std::cout << std::endl;
      }
      //@@@ END DEBUG

      x2 += 1;
    }
    y2 += 1;
  }
}
