#include "boolOps.hpp"

#include "voxelViewer.hpp"
#include "voxelizer.hpp"

// #define DEBUG_OUTPUT

// PROMPT
// I have a voxelized object stored in a file, which contains voxelization parameters, compressed data, and prefix sum data.
// The compressed data represent the transitions in the voxel grid along the Z-axis, and the prefix sum data is used to quickly access the compressed data for
// each XY column. In particular, prefix sum data total number of values is equal to the number of XY columns, and each value in the prefix sum data indicates
// the starting index of the compressed transitions for that XY column. E.g. if the prefix sum data is [0, 3, 5, 8], it means:
// - For the first XY column (0, 0), there are 3 transitions starting at index 0 in the compressed data.
// - For the second XY column (1, 0), there are 2 transitions starting at index 3 in the compressed data.
// - For the third XY column (2, 0), there are 3 transitions starting at index 5 in the compressed data.
// - For the fourth XY column (3, 0), there are 3 transitions starting at index 8 in the compressed data.
// Thus, to know where the transition data for the column [x, y] are in prefix sum data, we can use the formula:
// prefixSumData[x + y * resolutionXYZ.x], which gives the starting index of the compressed transitions for that XY column.
// The compressed data is stored as a vector of GLuints, where each transition is represented by a single GLuint. The unused values representing potential
// transitions, used during voxelization (thus maxTransitionsPerZColumn parameter) have been removed, so the compressed data only contains the actual
// transitions. The paameter maxTransitionsPerZColumn can be ignored for the purpose of loading the object, as it is only used during voxelization to limit the
// number of transitions per column. The voxelization parameters include the resolution (in fractions of the unit used by the user per voxel), resolutionXYZ,
// slicesPerBlock, maxMemoryBudgetBytes, maxTransitionsPerZColumn, color, and preview flag.

// I want to load two objects of this kind into memory and perform boolean operations on it, such as subtraction.
// The load function has been already implemented, and it reads the voxelization parameters, compressed data, and prefix sum data from the file.
// I can read e.g. two objects, now i want to implement the subtraction function: object1 - object2. Object 2 can be positioned arbitrarily with
// respect to object 1, i.e. it can be freely shifted along the X, Y, and Z axes as specified by the user.
// The world coordinates are considered to start at (0, 0, 0) of object 1, and object 2 can be positioned anywhere in the world space.
// The origin of object 1 is at (0, 0, 0) of its own coordinate system, and the origin of object 2 is at (0, 0, 0) of its own coordinate system.
// Shortly i will add an offset in the parameters of each object to specify a modified origin of the object in the world space, but for now this has not been
// implemented yet. The result should be stored in a new VoxelObject.

// Please implement the subtact function in the BoolOps class, using this template:
// bool BoolOps::subtract(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset) {}
// Eventually add more parametersto the function, if you think they are needed.

// VoxelObject is defined in the BoolOps class, and it contains the voxelization parameters, compressed data, and prefix sum data as follows:
//    struct VoxelObject {
//      VoxelizationParams params;
//      std::vector<GLuint> compressedData;
//      std::vector<GLuint> prefixSumData;
//    };

// VoxelizationParams is defined in the voxelizer.hpp file, and it contains the following fields:
//    struct VoxelizationParams {
//      float resolution = 0.1; // Resolution referred to the units of the model (agnostic with reference to which units are used)
//      glm::ivec3 resolutionXYZ = glm::ivec3(1024, 1024, 1024); // Resolution in pixels for each axis

//      int slicesPerBlock = 32;
//      size_t maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB
//      int maxTransitionsPerZColumn = 32;
//      glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f); // Default color (white)

//      bool preview = false; // Whether to render a preview during voxelization
//    };

bool BoolOps::load(const std::string& filename) {
  std::ifstream file(filename, std::ios::binary);
  if (!file) {
#ifdef DEBUG_OUTPUT
    std::cerr << "Failed to open file for reading: " << filename << std::endl;
#endif
    return false;
  }

  VoxelObject obj;

  // Read VoxelizationParams
  file.read(reinterpret_cast<char*>(&obj.params), sizeof(VoxelizationParams));
  if (!file) {
#ifdef DEBUG_OUTPUT
    std::cerr << "Failed to read params from file: " << filename << std::endl;
#endif
    return false;
  }

  // Read the rest of the file into memory
  file.seekg(0, std::ios::end);                           // Move to the end of the file
  size_t fileSize = file.tellg();                         // Get the size of the file
  file.seekg(sizeof(VoxelizationParams), std::ios::beg);  // Move back to the position after params

  size_t dataSize = 0;
  size_t prefixSize = 0;

  file.read(reinterpret_cast<char*>(&dataSize), sizeof(size_t));
  file.read(reinterpret_cast<char*>(&prefixSize), sizeof(size_t));

  if (!file) {
#ifdef DEBUG_OUTPUT
    std::cerr << "Failed to read data sizes from file: " << filename << std::endl;
#endif
    return false;
  }

  // Sanity check
  if (fileSize != sizeof(VoxelizationParams) + 2 * sizeof(size_t) + dataSize + prefixSize) {
#ifdef DEBUG_OUTPUT
    std::cerr << "File size mismatch for " << filename << std::endl;
#endif
    return false;
  } else {
#ifdef DEBUG_OUTPUT
    std::cout << "File size matches expected size." << std::endl;
#endif
  }

  // Read the compressed data and prefix sum data
  obj.compressedData.resize(dataSize / sizeof(GLuint));
  obj.prefixSumData.resize(prefixSize / sizeof(GLuint));
  file.read(reinterpret_cast<char*>(obj.compressedData.data()), dataSize);
  file.read(reinterpret_cast<char*>(obj.prefixSumData.data()), prefixSize);
  if (!file) {
    std::cerr << "Failed to read data from file: " << filename << std::endl;
    return false;
  }

#ifdef DEBUG_OUTPUT
  std::cout << "fileSize: " << fileSize << std::endl;
  std::cout << "VoxelizationParams:" << std::endl;
  std::cout << "  resolutionXYZ: (" << obj.params.resolutionXYZ.x << ", " << obj.params.resolutionXYZ.y << ", " << obj.params.resolutionXYZ.z << ")"
            << std::endl;
  std::cout << "  resolution: " << obj.params.resolution << std::endl;
  std::cout << "  dataSize: " << dataSize << std::endl;
  std::cout << "  prefixSize: " << prefixSize << std::endl;
#endif

  this->objects.push_back(std::move(obj));

  size_t objIndex = this->objects.size() - 1;
  std::cout << "Object successfully loaded from file: " << filename << " at index " << objIndex << std::endl;

  return true;
}

bool BoolOps::save(const std::string& filename, int idx) {
  if (idx < 0 || idx >= static_cast<int>(this->objects.size())) {
    std::cerr << "Invalid object index: " << idx << ". Valid range is [0, " << this->objects.size() - 1 << "]." << std::endl;
    return false;
  }

  const VoxelObject& obj = this->objects[idx];

  if (obj.compressedData.empty() || obj.prefixSumData.empty()) {
    std::cerr << "No data to save. Run voxelization first." << std::endl;
    return false;
  }

  std::ofstream file(filename, std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open file for writing: " << filename << std::endl;
    return false;
  }

  // Save params first
  file.write(reinterpret_cast<const char*>(&obj.params), sizeof(VoxelizationParams));
  if (!file) {
    std::cerr << "Failed to write params to file: " << filename << std::endl;
    return false;
  }

  size_t dataSize = obj.compressedData.size() * sizeof(GLuint);
  size_t prefixSize = obj.prefixSumData.size() * sizeof(GLuint);

  std::cout << "Data size write (compressedData): " << dataSize << " bytes\n";
  std::cout << "Prefix size write (prefixSumData): " << prefixSize << " bytes\n";

  file.write(reinterpret_cast<const char*>(&dataSize), sizeof(size_t));
  file.write(reinterpret_cast<const char*>(&prefixSize), sizeof(size_t));

  file.write(reinterpret_cast<const char*>(obj.compressedData.data()), dataSize);
  file.write(reinterpret_cast<const char*>(obj.prefixSumData.data()), prefixSize);

  if (!file) {
    throw std::runtime_error("Failed to write data to file: " + filename);
  }

  file.close();

  return true;
}

bool BoolOps::subtract_old(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset) {
  VoxelObject result;
  result.params = obj1.params;
  const glm::ivec3& res1 = obj1.params.resolutionXYZ;
  const glm::ivec3& res2 = obj2.params.resolutionXYZ;

  // Lambda for 2D to 1D index mapping
  const auto index = [](int x, int y, int width) { return x + y * width; };

  // Resize prefix sum data for the result object
  result.prefixSumData.resize(res1.x * res1.y);
  std::vector<GLuint> outputTransitions;  // Stores all compressed Z-transitions

  // Iterate through each (x,y) column of the result grid
  for (int y = 0; y < res1.y; ++y) {
    for (int x = 0; x < res1.x; ++x) {
      // Determine the 1D index for obj1's data
      uint idx1 = index(x, y, res1.x);
      GLuint start1 = obj1.prefixSumData[idx1];
      GLuint end1 = (idx1 + 1 < obj1.prefixSumData.size()) ? obj1.prefixSumData[idx1 + 1] : static_cast<GLuint>(obj1.compressedData.size());

      // Extract obj1's Z-transitions for the current column
      std::vector<GLuint> z1;
      if (end1 > start1) {
        z1.assign(obj1.compressedData.begin() + start1, obj1.compressedData.begin() + end1);
      }

      // Calculate obj2's local (x,y) coordinates relative to obj1's offset
      int x2 = x - offset.x;
      int y2 = y - offset.y;

      int initialBState = 0;
      std::vector<GLuint> z2_filtered_for_grid;

      if (x2 >= 0 && y2 >= 0 && x2 < res2.x && y2 < res2.y) {
        uint idx2 = index(x2, y2, res2.x);
        GLuint start2 = obj2.prefixSumData[idx2];
        GLuint end2 = (idx2 + 1 < obj2.prefixSumData.size()) ? obj2.prefixSumData[idx2 + 1] : static_cast<GLuint>(obj2.compressedData.size());

        if (end2 > start2) {
          for (auto it = obj2.compressedData.begin() + start2; it != obj2.compressedData.begin() + end2; ++it) {
            int shifted_z = static_cast<int>(*it) + offset.z;
            if (shifted_z < 0) {
              initialBState = 1 - initialBState;
            } else {
              break;
            }
          }

          for (auto it = obj2.compressedData.begin() + start2; it != obj2.compressedData.begin() + end2; ++it) {
            int shifted_z = static_cast<int>(*it) + offset.z;
            if (shifted_z >= 0 && shifted_z < res1.z) {
              z2_filtered_for_grid.push_back(static_cast<GLuint>(shifted_z));
            }
          }
        }
      }
      std::vector<GLuint> z2 = std::move(z2_filtered_for_grid);

      std::vector<std::pair<GLuint, int>> events;
      for (GLuint z : z1) events.emplace_back(z, 0);  // Type 0 = obj1 (A)
      for (GLuint z : z2) events.emplace_back(z, 1);  // Type 1 = obj2 (B)

      std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first < b.first;
        return a.second < b.second;
      });

      std::vector<GLuint> merged;  // Final list of transitions for the result object
      int aState = 0;              // Current state of obj1 (A) for this column
      int bState = initialBState;  // Current state of obj2 (B) for this column

      // Initialize the result's state based on the state BEFORE the first event.
      int currentResultState = (aState && !bState) ? 1 : 0;

      size_t i = 0;
      while (i < events.size()) {
        GLuint current_z = events[i].first;

        // Process each event at current_z individually, respecting the sort order.
        // This is the core fix: no more batching with aCount/bCount.
        while (i < events.size() && events[i].first == current_z) {
          if (events[i].second == 0) {  // Type A event
            aState = 1 - aState;
          } else {  // Type B event
            bState = 1 - bState;
          }
          i++;  // Move to the next event
        }

        // Calculate the new result state AFTER all events on this Z-plane are handled
        int newResultState = (aState && !bState) ? 1 : 0;

        // If the resulting state is different from the state before this Z-plane,
        // it means a net transition occurred and we should record it.
        if (newResultState != currentResultState) {
          merged.push_back(current_z);
          currentResultState = newResultState;  // Update the running state for the next plane
        }
      }

      result.prefixSumData[idx1] = static_cast<GLuint>(outputTransitions.size());
      outputTransitions.insert(outputTransitions.end(), merged.begin(), merged.end());
    }
  }

  result.compressedData = std::move(outputTransitions);
  const_cast<VoxelObject&>(obj1) = std::move(result);

  return true;
}

bool BoolOps::subtract(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset) {
  long w1 = static_cast<long>(obj1.params.resolutionXYZ.x);
  long h1 = static_cast<long>(obj1.params.resolutionXYZ.y);
  long z1 = static_cast<long>(obj1.params.resolutionXYZ.z);
  long w2 = static_cast<long>(obj2.params.resolutionXYZ.x);
  long h2 = static_cast<long>(obj2.params.resolutionXYZ.y);
  long z2 = static_cast<long>(obj2.params.resolutionXYZ.z);

  long translateX = w1 / 2 + offset.x;
  long translateY = h1 / 2 + offset.y;
  // long translateZ = z1 / 2 + offset.z;  //%%%%%
  long translateZ = z1 / 2 - offset.z;  // This is a convention change to match the original logic used during voxelization.

  long minX = glm::clamp(translateX - w2 / 2, 0L, w1);
  long maxX = glm::clamp(translateX + w2 / 2, 0L, w1);
  long minY = glm::clamp(translateY - h2 / 2, 0L, h1);
  long maxY = glm::clamp(translateY + h2 / 2, 0L, h1);

  long minZ = glm::clamp(translateZ - z2 / 2, 0L, z1 - 1);
  long maxZ = glm::clamp(translateZ + z2 / 2, 0L, z1 - 1);

  if (minX >= maxX || minY >= maxY || minZ >= maxZ) {
    std::cerr << "AOI is empty, no pixels to process." << std::endl;
    return false;
  }

  std::vector<GLuint> compressedDataNew;
  std::vector<GLuint> prefixSumDataNew(obj1.prefixSumData.size(), 0);
  size_t currentOffset = 0;

  for (long y1 = 0; y1 < h1; ++y1) {
    for (long x1 = 0; x1 < w1; ++x1) {
      long idx1 = x1 + y1 * w1;

      if (x1 >= minX && x1 < maxX && y1 >= minY && y1 < maxY) {
        // AOI: perform subtraction
        size_t start1 = obj1.prefixSumData[idx1];
        size_t end1 = (static_cast<size_t>(idx1) + 1 < obj1.prefixSumData.size()) ? obj1.prefixSumData[idx1 + 1] : obj1.compressedData.size();
        std::vector<long> packetZ1(obj1.compressedData.begin() + start1, obj1.compressedData.begin() + end1);

        //%%%%%%
        if (packetZ1.empty()) {
          // No geometry in obj1 at this column → nothing to subtract → leave empty
          prefixSumDataNew[idx1] = currentOffset;
          continue;
        }

        long x2 = x1 - (translateX - w2 / 2);
        long y2 = y1 - (translateY - h2 / 2);
        long idx2 = x2 + y2 * w2;

        std::vector<long> packetZ2;
        if (x2 >= 0 && x2 < w2 && y2 >= 0 && y2 < h2) {
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
            z += translateZ - z2 / 2;  // Note: z can be also outside the range of obj1, so we need to filter it later!!!
          }
        }

        // Combine transitions
        std::vector<std::pair<long, int>> combined;
        for (auto z : packetZ1) combined.emplace_back(z, 0);
        for (auto z : packetZ2) combined.emplace_back(z, 1);

        std::sort(combined.begin(), combined.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first || (a.first == b.first && a.second > b.second); });

        std::vector<long> result;
        bool blackOn = false;
        bool redOn = false;
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

        // Filter out-of-bounds
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

  // Directly assign the new data to obj1 (if obj1 is non-const and assignment is allowed)
  // If obj1 is const, you must use const_cast as before.
  const_cast<VoxelObject&>(obj1).compressedData = std::move(compressedDataNew);
  const_cast<VoxelObject&>(obj1).prefixSumData = std::move(prefixSumDataNew);

  return true;
}
