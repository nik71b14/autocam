#include <iostream>
#include <stdexcept>
#include "meshLoader.hpp"
#include "voxelViewer.hpp"
#include "voxelizer.hpp"
#include "voxelizerUtils.hpp"
#include "boolOps.hpp"
#include <assimp/scene.h>

//#define DEBUG_OUTPUT

const char* STL_PATH = "models/model3_bin.stl";
//const char* STL_PATH = "models/cone.stl";
//const char* STL_PATH = "models/cube.stl";
//const char* STL_PATH = "models/single_face_xy.stl";

int main(int argc, char** argv) {

  const char* stlPath = (argc > 1) ? argv[1] : STL_PATH;
  std::cout << "Using STL path: " << stlPath << std::endl;

  try {

    VoxelizationParams params;
    params.resolution = 0.02; // e.g. mm
    params.color = glm::vec3(0.1f, 0.4f, 0.8f); // Blue color for voxelization
    params.maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB
    params.slicesPerBlock = chooseOptimalPowerOfTwoSlicesPerBlock(params);
    //params.slicesPerBlock = chooseOptimalSlicesPerBlock(params.resolution, params.resolutionXYZ.z, params.maxMemoryBudgetBytes);
    //params.preview = true; // Enable preview during voxelization

    Mesh mesh = loadMesh(stlPath);

    // 1. Stack-allocate the Voxelizer object, run the voxelization, and save results
    Voxelizer* voxelizer = new Voxelizer(mesh, params);
    voxelizer->run();
    voxelizer->save("test/voxelized_obj_1.bin"); // Save results to a binary file
    auto [compressedData, prefixSumData] = voxelizer->getResults();

    // Heap-allocated Voxelizer object (alternative)
    // Voxelizer voxelizer(mesh, params);
    // voxelizer.run();
    // auto [compressedData, prefixSumData] = voxelizer.getResults();

    #ifdef DEBUG_OUTPUT
    std::cout << "Optimal slices per block (power of 2): " << params.slicesPerBlock << std::endl;
    std::cout << "Loaded mesh: " << stlPath << std::endl;
    std::cout << "Number of vertices: " << mesh.vertices.size() / 3 << std::endl;
    std::cout << "Number of faces: " << mesh.indices.size() / 3 << std::endl;
    std::cout << "Computed scale: " << voxelizer->getScale() << std::endl;
    glm::ivec3 res = voxelizer->getResolutionPx();
    std::cout << "Voxelization resolution: " 
          << res.x << " (X) x " 
          << res.y << " (Y) x " 
          << res.z << " (Z) [px]" << std::endl;
    std::cout << "Voxelization resolution: "
          << voxelizer->getResolution() << " [object units]" << std::endl;
    #endif

    delete voxelizer;

    // 2. Load the voxelized object from the binary file
    BoolOps* ops = new BoolOps();
    // Loads object 1
    if (!ops->load("test/voxelized_obj_1.bin")) {
      std::cerr << "Failed to load voxelized object." << std::endl;
    }
    // Loads object 2
    if (!ops->load("test/voxelized_obj_1.bin")) {
      std::cerr << "Failed to load voxelized object." << std::endl;
    }
    ops->clear();
    delete ops;

    exit(EXIT_SUCCESS);

    /*
    //params.resolutionXYZ = voxelizer->getResolutionPx(); // Not needed since params.resolutionXYZ is already set in the VoxelizationParams constructor
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
    */
  } catch (const std::exception& e) {
    std::cerr << "[Error] " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
