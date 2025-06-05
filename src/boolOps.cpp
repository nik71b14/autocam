#include "boolOps.hpp"
#include "voxelizer.hpp"

//#define DEBUG_OUTPUT

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
    file.seekg(0, std::ios::end); // Move to the end of the file
    size_t fileSize = file.tellg(); // Get the size of the file
    file.seekg(sizeof(VoxelizationParams), std::ios::beg); // Move back to the position after params

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
    std::cout << "  resolutionXYZ: (" << obj.params.resolutionXYZ.x << ", " << obj.params.resolutionXYZ.y << ", " << obj.params.resolutionXYZ.z << ")" << std::endl;
    std::cout << "  resolution: " << obj.params.resolution << std::endl;
    std::cout << "  dataSize: " << dataSize << std::endl;
    std::cout << "  prefixSize: " << prefixSize << std::endl;
    #endif

    this->objects.push_back(std::move(obj));

    size_t objIndex = this->objects.size() - 1;
    std::cout << "Object successfully loaded from file: " << filename << " at index " << objIndex << std::endl;

    return true;
}
