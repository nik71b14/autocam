#include <iostream>
#include <stdexcept>
#include "meshLoader.hpp"
#include "voxelViewer.hpp"
#include "voxelizer.hpp"
#include "voxelizerUtils.hpp"

#define DEBUG_OUTPUT

const char* STL_PATH = "models/model3_bin.stl";
//const char* STL_PATH = "models/cone.stl";
//const char* STL_PATH = "models/cube.stl";
//const char* STL_PATH = "models/single_face_xy.stl";

int main(int argc, char** argv) {

  const char* stlPath = (argc > 1) ? argv[1] : STL_PATH;

  try {

    VoxelizationParams params;
    params.resolution = 1024;
    params.resolutionZ = 1024; // Default Z resolution
    params.maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB

    params.slicesPerBlock = chooseOptimalPowerOfTwoSlicesPerBlock(params);
    //params.slicesPerBlock = chooseOptimalSlicesPerBlock(params.resolution, params.resolutionZ, params.maxMemoryBudgetBytes);

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    loadMesh(stlPath, vertices, indices);

    #ifdef DEBUG_OUTPUT
    std::cout << "Optimal slices per block (power of 2): " << params.slicesPerBlock << std::endl;
    std::cout << "Loaded mesh: " << stlPath << std::endl;
    std::cout << "Number of vertices: " << vertices.size() / 3 << std::endl;
    std::cout << "Number of faces: " << indices.size() / 3 << std::endl;
    #endif

    Voxelizer voxelizer(vertices, indices, params);
    voxelizer.run();
    auto [compressedData, prefixSumData] = voxelizer.getResults();

    VoxelViewer viewer(
      compressedData,
      prefixSumData,
      params.resolution,
      params.resolutionZ,
      params.maxTransitionsPerZColumn
    );
    viewer.run();

    // VoxelViewer viewer(
    //     "test/cross_compressedBuffer.bin",
    //     "test/cross_prefixSumBuffer.bin",
    //     200,
    //     200,
    //     32
    //   );
    // viewer.run();
    
  } catch (const std::exception& e) {
    std::cerr << "[Error] " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
