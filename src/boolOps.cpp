#include "boolOps.hpp"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <chrono>

#include "voxelViewer.hpp"
#include "voxelizer.hpp"

// #define DEBUG_OUTPUT
#define DEBUG_SPEED_OUTPUT
#define WORKGROUPS 8

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

GLuint BoolOps::createBuffer(GLsizeiptr size, GLuint binding, GLenum usage) {
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, usage);
  return buffer;
}

GLuint BoolOps::createBuffer(GLsizeiptr size, GLuint binding, GLenum usage, const GLuint* data) {
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, usage);
  return buffer;
}

void BoolOps::loadBuffer(GLuint binding, const std::vector<GLuint>& data) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, binding);
  glBufferData(GL_SHADER_STORAGE_BUFFER, data.size() * sizeof(GLuint), data.data(), GL_STATIC_READ);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

std::vector<GLuint> BoolOps::readBuffer(GLuint binding, size_t numElements) {
  std::vector<GLuint> data(numElements);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, binding);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, numElements * sizeof(GLuint), data.data());
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  return data;
}

GLuint BoolOps::createAtomicCounter(GLuint binding) {
  GLuint buffer;
  glGenBuffers(1, &buffer);
  glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, binding, buffer);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, buffer);
  GLuint zero = 0;
  glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &zero, GL_DYNAMIC_DRAW);
  return buffer;
}

void BoolOps::zeroAtomicCounter(GLuint binding) {
  GLuint zero = 0;
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, binding);
  glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
}

void BoolOps::zeroBuffer(GLuint binding) {
  GLint size = 0;
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, binding);
  glGetBufferParameteriv(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_SIZE, &size);
  if (size > 0) {
    std::vector<GLuint> zeros(size / sizeof(GLuint), 0);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size, zeros.data());
  }
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

GLuint BoolOps::readAtomicCounter(GLuint binding) {
  GLuint count = 0;
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, binding);
  glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &count);
  glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
  return count;
}

GLFWwindow* BoolOps::createGLContext() {
  if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow* window = glfwCreateWindow(1, 1, "Hidden", nullptr, nullptr);
  if (!window) throw std::runtime_error("Failed to create GLFW window");
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) throw std::runtime_error("Failed to init GLAD");

  #ifdef DEBUG_OUTPUT
  std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
  std::cout << "GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
  #endif

  return window;
}

void BoolOps::destroyGLContext(GLFWwindow* window) {
  if (window) {
    glfwDestroyWindow(window);
    glfwTerminate();
  }
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

bool BoolOps::subtractGPU(const VoxelObject& obj1, const VoxelObject& obj2, glm::ivec3 offset) {
  // The subtract() CPU code:
  // ✅ Iterates over each (x,y) column.
  // ✅ Gathers Z transitions for both objects.
  // ✅ Merges/sorts/evaluates transitions.
  // ✅ Generates a new compressed result.

  if (!glContext) glContext = createGLContext();

  // Load and compile shader using Shader class
  Shader shader("shaders/subtract.comp");  // Path to your compute shader file
  shader.use();

  // auto start = std::chrono::high_resolution_clock::now();  // ====> Start timing

  // Calculate parameters
  long w1 = obj1.params.resolutionXYZ.x;
  long h1 = obj1.params.resolutionXYZ.y;
  long z1 = obj1.params.resolutionXYZ.z;
  long w2 = obj2.params.resolutionXYZ.x;
  long h2 = obj2.params.resolutionXYZ.y;
  long z2 = obj2.params.resolutionXYZ.z;

  long translateX = w1 / 2 + offset.x;
  long translateY = h1 / 2 + offset.y;
  long translateZ = z1 / 2 - offset.z;

  size_t prefixCount = w1 * h1;
  size_t outSizeEstimate = obj1.compressedData.size() + obj2.compressedData.size();

  // Set uniforms
  shader.setInt("w1", w1);
  shader.setInt("h1", h1);
  shader.setInt("z1", z1);
  shader.setInt("w2", w2);
  shader.setInt("h2", h2);
  shader.setInt("z2", z2);
  shader.setInt("translateX", translateX);
  shader.setInt("translateY", translateY);
  shader.setInt("translateZ", translateZ);
  shader.setUInt("maxTransitions", 256);

  // Create buffers

  // IN
  GLuint obj1Compressed = createBuffer(obj1.compressedData.size() * sizeof(GLuint), 0, GL_STATIC_READ);  // Load data later
  GLuint obj1Prefix = createBuffer(obj1.prefixSumData.size() * sizeof(GLuint), 1, GL_STATIC_READ);       // Load data later
  GLuint obj2Compressed = createBuffer(obj2.compressedData.size() * sizeof(GLuint), 2, GL_STATIC_READ);
  GLuint obj2Prefix = createBuffer(obj2.prefixSumData.size() * sizeof(GLuint), 3, GL_STATIC_READ);
  // OUT
  GLuint outCompressed = createBuffer(outSizeEstimate * sizeof(GLuint), 4, GL_DYNAMIC_COPY, nullptr);
  GLuint outPrefix = createBuffer(prefixCount * sizeof(GLuint), 5, GL_DYNAMIC_COPY, nullptr);

  GLuint atomicCounter = createAtomicCounter(6);
  zeroAtomicCounter(6);  // Initialize atomic counter to zero

#ifdef DEBUG_OUTPUT
  GLuint debugCounter = createAtomicCounter(7);
  zeroAtomicCounter(7);  // Initialize atomic counter to zero
#endif

  // Setup dispatch parameters
  GLuint groupsX = (GLuint)((w1 + (WORKGROUPS - 1)) / WORKGROUPS);  // Assuming WORKGROUPS threads per work group in X, rounding up
  GLuint groupsY = (GLuint)((h1 + (WORKGROUPS - 1)) / WORKGROUPS);  // Assuming WORKGROUPS threads per work group in Y, rounding up
#ifdef DEBUG_OUTPUT
  std::cout << "Dispatching " << groupsX << " x " << groupsY << " work groups (" << w1 << "x" << h1 << " pixels)\n";
#endif

#ifdef DEBUG_SPEED_OUTPUT
  auto start1 = std::chrono::high_resolution_clock::now();  // ====> Start timing
#endif

  // Load buffers (about 3.5 ms for 1M transitions)
  // NOTE: by not loading data each time, reusing obj1 buffer each time a subtraction is performed,
  // the execution time is reduced to about 0.5 ms for 1M transitions (improvement by 7x)
  loadBuffer(obj1Compressed, obj1.compressedData);  // Load data into the buffer
  loadBuffer(obj1Prefix, obj1.prefixSumData);       // Load prefix sum data into the buffer
  loadBuffer(obj2Compressed, obj2.compressedData);  // Load data into the buffer
  loadBuffer(obj2Prefix, obj2.prefixSumData);       // Load prefix sum

#ifdef DEBUG_SPEED_OUTPUT
  auto start2 = std::chrono::high_resolution_clock::now();  // ====> Start timing
#endif

  // Dispatch compute shader (about 0.5 ms for 1M transitions)
  glDispatchCompute(groupsX, groupsY, 1);
  glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

  auto end = std::chrono::high_resolution_clock::now();  // ====> End timing

  //@@@ DEBUG: Read test output -----------------------------------------------
  // std::vector<GLuint> outData(1);
  // glBindBuffer(GL_SHADER_STORAGE_BUFFER, outCompressed);
  // glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint), outData.data());
  // std::cout << "Compressed output[0]: 0x" << std::hex << outData[0] << std::endl;
  //@@@ END DEBUG -------------------------------------------------------------

  // Read atomic counter to get the number of written elements in outCompressed
  GLuint writtenCount = readAtomicCounter(atomicCounter);
#ifdef DEBUG_OUTPUT
  std::cout << "Atomic counter (written elements): " << std::dec << writtenCount << std::endl;
#endif

#ifdef DEBUG_OUTPUT
  //@@@ DEBUG:  Read debug atomic counter -------------------------------------
  GLuint debugCount = readAtomicCounter(debugCounter);
  std::cout << "Debug atomic counter: " << std::dec << debugCount << std::endl;
//@@@ END DEBUG -------------------------------------------------------------
#endif

  // Read output buffers
  std::vector<GLuint> compressedOut = readBuffer(outCompressed, writtenCount);
  std::vector<GLuint> prefixOut = readBuffer(outPrefix, prefixCount);

#ifdef DEBUG_OUTPUT
  //@@@ DEBUG: Print output data ----------------------------------------------
  std::cout << "First 30-40 values of compressedOut:" << std::endl;
  for (size_t i = 0; i < std::min<size_t>(compressedOut.size(), 40); ++i) {
    std::cout << compressedOut[i] << " ";
    if ((i + 1) % 10 == 0) std::cout << std::endl;
  }
  std::cout << std::endl;
//@@@ END DEBUG -------------------------------------------------------------
#endif

  // Overwrite obj1
  const_cast<VoxelObject&>(obj1).compressedData = std::move(compressedOut);
  const_cast<VoxelObject&>(obj1).prefixSumData = std::move(prefixOut);  // COPY JUST THIS FOR NOW

  // Cleanup
  glDeleteBuffers(1, &obj1Compressed);
  glDeleteBuffers(1, &obj1Prefix);
  glDeleteBuffers(1, &obj2Compressed);
  glDeleteBuffers(1, &obj2Prefix);
  glDeleteBuffers(1, &outCompressed);
  glDeleteBuffers(1, &outPrefix);
  glDeleteBuffers(1, &atomicCounter);

  destroyGLContext(glContext);

#ifdef DEBUG_SPEED_OUTPUT
  std::chrono::duration<double, std::milli> duration1 = end - start1;
  std::cout << "GPU subtraction (including loading buffers) took " << duration1.count() << " ms\n";
  std::chrono::duration<double, std::milli> duration2 = end - start2;
  std::cout << "GPU dispatch (excluding loading buffers) took " << duration2.count() << " ms\n";
#endif

  return true;
}
