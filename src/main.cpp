#include <iostream>
#include <stdexcept>
#include "meshLoader.hpp"
#include "voxelViewer.hpp"
#include "voxelizer.hpp"
#include "voxelizerUtils.hpp"
#include <assimp/scene.h>

#define DEBUG_OUTPUT

const char* STL_PATH = "models/model3_bin.stl";
//const char* STL_PATH = "models/cone.stl";
//const char* STL_PATH = "models/cube.stl";
//const char* STL_PATH = "models/single_face_xy.stl";

int main(int argc, char** argv) {

  const char* stlPath = (argc > 1) ? argv[1] : STL_PATH;

  try {

    VoxelizationParams params;
    params.resolution = 0.1; // E.g. mm
    params.resolutionZ = 1000; // Default Z resolution
    params.resolutionX = 1000;
    params.resolutionY = 1000;
    params.maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB

    params.slicesPerBlock = chooseOptimalPowerOfTwoSlicesPerBlock(params);
    //params.slicesPerBlock = chooseOptimalSlicesPerBlock(params.resolution, params.resolutionZ, params.maxMemoryBudgetBytes);

    Mesh mesh = loadMesh(stlPath);
    Voxelizer voxelizer(mesh, params);
    // voxelizer.run();
    voxelizer.run();
    auto [compressedData, prefixSumData] = voxelizer.getResults();

    #ifdef DEBUG_OUTPUT
    std::cout << "Optimal slices per block (power of 2): " << params.slicesPerBlock << std::endl;
    std::cout << "Loaded mesh: " << stlPath << std::endl;
    std::cout << "Number of vertices: " << mesh.vertices.size() / 3 << std::endl;
    std::cout << "Number of faces: " << mesh.indices.size() / 3 << std::endl;
    std::cout << "Computed scale: " << voxelizer.getScale() << std::endl;
    #endif

    VoxelViewer viewer(
      compressedData,
      prefixSumData,
      params
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
