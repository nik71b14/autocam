#include <iostream>
#include <stdexcept>
#include "meshLoader.hpp"
#include "voxelViewer.hpp"
#include "voxelizer.hpp"
#include "voxelizerUtils.hpp"
#include "boolOps.hpp"
#include <assimp/scene.h>
#include "gcode.hpp"
#include "gcodeViewer.hpp"
#include "main_params.hpp"
#include "utils.hpp"

//#define GCODE_TESTING
//#define VOXELIZATION_TESTING
//#define BOOLEAN_OPERATIONS_TESTING
#define VOXEL_VIEWER_TESTING


int main(int argc, char** argv) {

  const char* stlPath = (argc > 1) ? argv[1] : STL_PATH;
  std::cout << "Using STL path: " << stlPath << std::endl;

  try {

    // VOXELIZATION PARAMETERS ------------------------------------------------
    VoxelizationParams params;
    params.resolution = 0.2; // e.g. mm
    params.color = glm::vec3(0.1f, 0.4f, 0.8f); // Blue color for voxelization
    params.maxMemoryBudgetBytes = 512 * 1024 * 1024; // 512 MB
    params.slicesPerBlock = chooseOptimalPowerOfTwoSlicesPerBlock(params);
    //params.slicesPerBlock = chooseOptimalSlicesPerBlock(params.resolution, params.resolutionXYZ.z, params.maxMemoryBudgetBytes);
    //params.preview = true; // Enable preview during voxelization
    // ------------------------------------------------------------------------


    // GCODE INTERPRETER TESTING ----------------------------------------------
    #ifdef GCODE_TESTING
    GCodeInterpreter interpreter;

    if (!interpreter.loadFile(GCODE_PATH)) {
        std::cerr << "Failed to load G-code file.\n";
        return 1;
    }

    if (!interpreter.checkFile()) {
        std::cerr << "Invalid G-code file.\n";
        return 1;
    }

    // Extract full toolpath for initial rendering
    std::vector<GcodePoint> toolpath = interpreter.getToolpath();
    GcodeViewer gCodeViewer(toolpath);

    interpreter.setSpeedFactor(SPEED_FACTOR); // Run 2x faster than real time
    interpreter.run();

    while (interpreter.isRunning()) {
        glm::vec3 pos = interpreter.getCurrentPosition();

        #ifdef MAIN_DEBUG_OUTPUT
        std::cout << "Tool Position: X=" << pos.x << " Y=" << pos.y << " Z=" << pos.z << std::endl;
        #endif

        // Update viewer with current tool position
        gCodeViewer.setToolPosition(pos);

        // Handle input and draw
        gCodeViewer.pollEvents();
        gCodeViewer.drawFrame();

        //std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Simulation finished.\n";
    exit(EXIT_SUCCESS);
    #endif // GCODE_TESTING
    // ------------------------------------------------------------------------

    // VOXELIZATION TESTING ---------------------------------------------------
    #if defined(VOXELIZATION_TESTING) || defined(VOXEL_VIEWER_TESTING)
    Mesh mesh = loadMesh(stlPath);

    // 1. Stack-allocate the Voxelizer object, run the voxelization, and save results
    Voxelizer* voxelizer = new Voxelizer(mesh, params);
    voxelizer->run();
    
    //voxelizer->save("test/test.bin"); // Save results to a binary file
    voxelizer->save("test/" + stlToBinName(getFileNameFromPath(STL_PATH))); // Save results to a binary file

    auto [compressedData, prefixSumData] = voxelizer->getResults();

    // Heap-allocated Voxelizer object (alternative)
    // Voxelizer voxelizer(mesh, params);
    // voxelizer.run();
    // auto [compressedData, prefixSumData] = voxelizer.getResults();

    #ifdef MAIN_DEBUG_OUTPUT
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
    #endif // VOXELIZATION_TESTING
    // ------------------------------------------------------------------------

    // BOOLEAN OPERATIONS TESTING ---------------------------------------------
    #ifdef BOOLEAN_OPERATIONS_TESTING
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
    ops->subtract(ops->getObjects()[0], ops->getObjects()[1], glm::ivec3(100, 100, 100));
    ops->clear();
    if (ops) delete ops;
    exit(EXIT_SUCCESS);
    #endif // BOOLEAN_OPERATIONS_TESTING
    // ------------------------------------------------------------------------

    // VOXEL VIEWER TESTING----------------------------------------------------
    #ifdef VOXEL_VIEWER_TESTING
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

    exit(EXIT_SUCCESS);
    #endif // VOXEL_VIEWER_TESTING
    // ------------------------------------------------------------------------

  } catch (const std::exception& e) {
    std::cerr << "[Error] " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
