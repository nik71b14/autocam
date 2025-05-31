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
    params.resolution = 0.05; // E.g. mm
    params.maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB
    //params.slicesPerBlock = 8;
    params.slicesPerBlock = chooseOptimalPowerOfTwoSlicesPerBlock(params);
    //params.slicesPerBlock = chooseOptimalSlicesPerBlock(params.resolution, params.resolutionZ, params.maxMemoryBudgetBytes);
    //params.preview = true; // Enable preview during voxelization

    Mesh mesh = loadMesh(stlPath);
    Voxelizer voxelizer(mesh, params);
    voxelizer.run();
    auto [compressedData, prefixSumData] = voxelizer.getResults();

    #ifdef DEBUG_OUTPUT
    std::cout << "Optimal slices per block (power of 2): " << params.slicesPerBlock << std::endl;
    std::cout << "Loaded mesh: " << stlPath << std::endl;
    std::cout << "Number of vertices: " << mesh.vertices.size() / 3 << std::endl;
    std::cout << "Number of faces: " << mesh.indices.size() / 3 << std::endl;
    std::cout << "Computed scale: " << voxelizer.getScale() << std::endl;
    glm::ivec3 res = voxelizer.getResolutionPx();
    std::cout << "Voxelization resolution: " 
          << res.x << " (X) x " 
          << res.y << " (Y) x " 
          << res.z << " (Z) [px]" << std::endl;
    std::cout << "Voxelization resolution: "
          << voxelizer.getResolution() << " [object units]" << std::endl;
    #endif

    params.resolutionX = res.x;
    params.resolutionY = res.y;
    params.resolutionZ = res.z;

    VoxelViewer viewer(
      compressedData,
      prefixSumData,
      params
    );
    viewer.setOrthographic(false); // Set orthographic projection
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
