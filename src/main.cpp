#include <assimp/scene.h>

#include <cmath>  // for sin()
#include <iostream>
#include <stdexcept>

#include "boolOps.hpp"
#include "gcode.hpp"
#include "gcodeViewer.hpp"
#include "main_params.hpp"
#include "meshLoader.hpp"
#include "test.hpp"
#include "utils.hpp"
#include "voxelViewer.hpp"
#include "voxelizer.hpp"
#include "voxelizerUtils.hpp"

// #define GCODE_TESTING
// #define VOXELIZATION_TESTING
// #define BOOLEAN_OPERATIONS_TESTING
// #define VOXEL_VIEWER_TESTING
// #define TEST
#define TEST_FLAT

int main(int argc, char** argv) {
  const char* stlPath = (argc > 1) ? argv[1] : STL_PATH;
  std::cout << "Using STL path: " << stlPath << std::endl;

#ifdef TEST_FLAT
  BoolOps ops;

  // if (!ops.load("test/workpiece_100_100_50.bin")) {
  if (!ops.load("test/obj1.bin")) {
    std::cerr << "Failed to load voxelized object." << std::endl;
    return 1;
  }

  // if (!ops.load("test/hemispheric_mill_10.bin")) {
  if (!ops.load("test/hemispheric_mill_3.bin")) {
    // if (!ops.load("test/obj2.bin")) {
    std::cerr << "Failed to load voxelized object." << std::endl;
    return 1;
  }

  // Init objects for subtraction
  ops.subtractGPU_init(ops.getObjects()[0], ops.getObjects()[1]);

  // Spiral subtraction loop (round spiral in XY, descending in Z)
  float centerX = 0.0f;
  float centerY = 0.0f;
  float centerZ = 720.0f;

  float a = 0.0f;          // Spiral base radius
  float b = 50.0f;          // Spiral spacing (distance between loops)
  float angleStep = 0.01f;  // Angle increment (radians) — lower = smoother spiral
  float zStep = 0.25f;     // Z descent per step

  for (int i = 0; i < 1500; ++i) {
    float theta = i * angleStep;
    float r = a + b * theta;

    float x = centerX + r * std::cos(theta);
    float y = centerY + r * std::sin(theta);
    float z = centerZ - zStep * float(i);

    ops.subtractGPU(ops.getObjects()[0], glm::ivec3(std::round(x), std::round(y), std::round(z)));
  }

  //@@@ QUA VEDERE PERCHE' NON FUNZIONA CON VoxelObject result
  // Copy back the result
  // VoxelObject result;
  ops.subtractGPU_copyback(ops.getObjects()[0]);
  // ops.subtractGPU_copyback(result);

  //@@@ QUA USARE IN INPUT DEL VIEWER DIRETTAMENTE UN VoxelObject
  VoxelViewer viewer(ops.getObjects()[0].compressedData, ops.getObjects()[0].prefixSumData, ops.getObjects()[0].params);
  // VoxelViewer viewer(result.compressedData, result.prefixSumData, result.params);
  // viewer.setOrthographic(true);  // Set orthographic projection
  viewer.run();

  exit(EXIT_SUCCESS);
#endif

#ifdef TEST
/*
  // analizeVoxelizedObject("test/point_mill_10.bin");

  // subtract("test/workpiece_100_100_50.bin", "test/hemispheric_mill_10.bin", glm::ivec3(0, 0, 820));

  BoolOps ops;

  // if (!ops.load("test/workpiece_100_100_50.bin")) {
  if (!ops.load("test/obj1.bin")) {
    std::cerr << "Failed to load voxelized object." << std::endl;
    return 1;
  }

  if (!ops.load("test/hemispheric_mill_10.bin")) {
    // if (!ops.load("test/obj2.bin")) {
    std::cerr << "Failed to load voxelized object." << std::endl;
    return 1;
  }

  // Perform subtraction
  // if (!ops.subtract(ops.getObjects()[0], ops.getObjects()[1], glm::ivec3(0, 0, 500))) {
  //   std::cerr << "Subtraction failed." << std::endl;
  //   return 1;
  // }

  // Perform subtraction using GPU --------------------------------------------
  // ops.test(ops.getObjects()[0], ops.getObjects()[1], glm::ivec3(0, 0, 0));

  // ops.setupSubtractBuffers(ops.getObjects()[0], ops.getObjects()[1]);  // Needed to setup buffers before GPU subtraction
  // if (!ops.subtractGPU(ops.getObjects()[0], ops.getObjects()[1], glm::ivec3(0, 0, 820))) {
  // // if (!ops.subtractGPU(ops.getObjects()[0], ops.getObjects()[1], glm::ivec3(0, 0, 500))) {
  // std::cerr << "Subtraction failed." << std::endl;
  // return 1;
  // }

  // =====> LAVORARE SU QUESTO ORA, BISOGNA inserire zeroAtomicCounter(atomicCounter); prima del dispatch
  // =====> DEVE GENERARE ANCHE il prefixSumData, che ora non viene generato
  // =====> Per farlo, devo usare un secondo shader che compatta i dati e genera il prefix sum
  // =====> Ho già questo shader: prefix_passn.comp

  // This cycle took 26 ms per subtraction on average on the Dell machine
  // for (int i = 0; i < 3; i++) {
  //   ops.setupSubtractBuffers(ops.getObjects()[0], ops.getObjects()[1]);  // Needed to setup buffers before GPU subtraction
  //   ops.subtractGPU(ops.getObjects()[0], ops.getObjects()[1], glm::ivec3(-250 + 250 * i, -250 + 250 * i, 820));
  // }

  // Test GPU sequence subtraction
  ops.setupSubtractBuffers(ops.getObjects()[0], ops.getObjects()[1]);  // Needed to setup buffers before GPU subtraction
  ops.subtractGPU_sequence(ops.getObjects()[0], ops.getObjects()[1], glm::ivec3(-250, -250, 820));

  // --------------------------------------------------------------------------

  VoxelViewer viewer(ops.getObjects()[0].compressedData, ops.getObjects()[0].prefixSumData, ops.getObjects()[0].params);
  // viewer.setOrthographic(true);  // Set orthographic projection
  viewer.run();

  exit(EXIT_SUCCESS);
*/
#endif

  try {
    // VOXELIZATION PARAMETERS ------------------------------------------------
    VoxelizationParams params;
    params.resolution = RESOLUTION;
    params.color = WHITE;
    params.maxMemoryBudgetBytes = MEM_512MB;
    params.slicesPerBlock = chooseOptimalPowerOfTwoSlicesPerBlock(params);
    // params.slicesPerBlock = chooseOptimalSlicesPerBlock(params.resolution, params.resolutionXYZ.z, params.maxMemoryBudgetBytes);
    // params.preview = true;  // Enable preview during voxelization
    //  ------------------------------------------------------------------------

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
    gCodeViewer.setProjectionType(ProjectionType::ORTHOGRAPHIC);  // Set orthographic projection
    gCodeViewer.setWorkpieceVO("test/workpiece_100_100_50.bin");  // Set workpiece .bin file
    gCodeViewer.setToolVO("models/hemispheric_mill_10.bin");      // Set tool .bin file)

    interpreter.setSpeedFactor(SPEED_FACTOR);  // Run 2x faster than real time
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
      gCodeViewer.carve(pos);  // Carve the workpiece with the tool
      gCodeViewer.drawFrame();

      // std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "Simulation finished.\n";
    exit(EXIT_SUCCESS);
#endif  // GCODE_TESTING
// ------------------------------------------------------------------------

// VOXELIZATION TESTING ---------------------------------------------------
#if defined(VOXELIZATION_TESTING)
    Mesh mesh = loadMesh(stlPath);

    // 1. Stack-allocate the Voxelizer object, run the voxelization, and save results
    Voxelizer* voxelizer = new Voxelizer(mesh, params);
    voxelizer->run();

    // voxelizer->save("test/test.bin"); // Save results to a binary file
    voxelizer->save("test/" + stlToBinName(getFileNameFromPath(STL_PATH)));  // Save results to a binary file

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
    std::cout << "Voxelization resolution: " << res.x << " (X) x " << res.y << " (Y) x " << res.z << " (Z) [px]" << std::endl;
    std::cout << "Voxelization resolution: " << voxelizer->getResolution() << " [object units]" << std::endl;
#endif

    // Get updated params
    params = voxelizer->getParams();

    delete voxelizer;
#endif  // VOXELIZATION_TESTING
// ------------------------------------------------------------------------

// BOOLEAN OPERATIONS TESTING ---------------------------------------------
#ifdef BOOLEAN_OPERATIONS_TESTING
    // 2. Load the voxelized object from the binary file
    BoolOps* ops = new BoolOps();
    // Loads object 1
    // if (!ops->load("test/workpiece_100_100_50.bin")) {
    if (!ops->load("test/obj1.bin")) {
      std::cerr << "Failed to load voxelized object." << std::endl;
    }
    // Loads object 2
    if (!ops->load("test/hemispheric_mill_10.bin")) {
      std::cerr << "Failed to load voxelized object." << std::endl;
    }

    int index = 0;
    // BASIC SUBTRACTION TESTING
    // ops->subtract(ops->getObjects()[0], ops->getObjects()[1], glm::ivec3(500.0f, 500.0f, -400.0f));
    // ops->subtract(ops->getObjects()[0], ops->getObjects()[1], glm::ivec3(0, 0, 600));

    // STRAIGHT LINE SUBTRACTION TESTING
    // for (float mov = 0.0f; mov < 200.0f; mov += 20.0f) {
    //   // Subtract the second object from the first with an offset
    //   ops->subtract(ops->getObjects()[0], ops->getObjects()[1], glm::ivec3(200.0f + mov, 200.0f + mov, -400.0f));
    //   index++;
    //   std::cout << "Subtraction operation " << index << " completed." << std::endl;
    // }

    // SINE OSCILLATION SUBTRACTION TESTING
    for (int mov = 0; mov < 400; mov += 10) {
      // base position along 45° diagonal
      int baseX = -300 + mov;
      int baseY = -300 + mov;

      // sine oscillation parameters
      float amplitude = 30.0f;  // adjust as needed
      float frequency = 5.0f;   // adjust as needed

      // sine offset perpendicular to 45° line
      float sineVal = amplitude * std::sin(frequency * mov);

      // since we want to offset along the vector perpendicular to (1,1) -> (-1,1)
      // we can compute:
      int offsetX = static_cast<int>(sineVal / std::sqrt(2.0f));  // normalize the vector (-1,1)
      int offsetY = static_cast<int>(-sineVal / std::sqrt(2.0f));

      glm::ivec3 offsetPos(baseX + offsetX, baseY + offsetY, 820 - mov / 4);
      // glm::ivec3 offsetPos(0, 0, 820);

      //@@@ SE LA SOTTRAZIONE RESTITUISCE FALSE, VEDERE COSA SUCCEDE

      ops->subtract(ops->getObjects()[0], ops->getObjects()[1], offsetPos);
      // ops->subtractGPU(ops->getObjects()[0], ops->getObjects()[1], offsetPos);
      index++;
      std::cout << "Subtraction operation " << index << " completed at "
                << "X=" << offsetPos.x << " Y=" << offsetPos.y << " Z=" << offsetPos.z << std::endl;
    }

    // this->objects[0].params.color = glm::vec3(1.0f, 0.8f, 0.6f);
    VoxelViewer viewer(ops->getObjects()[0].compressedData, ops->getObjects()[0].prefixSumData, ops->getObjects()[0].params);
    viewer.run();

    ops->clear();
    if (ops) delete ops;
    exit(EXIT_SUCCESS);
#endif  // BOOLEAN_OPERATIONS_TESTING
// ------------------------------------------------------------------------

// VOXEL VIEWER TESTING----------------------------------------------------
#ifdef VOXEL_VIEWER_TESTING
    BoolOps ops;
    if (!ops.load(BIN_PATH)) {
      std::cerr << "Failed to load voxelized object." << std::endl;
      return 1;
    }
    VoxelObject obj = ops.getObjects()[0];
    VoxelViewer viewer(obj.compressedData, obj.prefixSumData, obj.params);
    // viewer.setOrthographic(true);  // Set orthographic projection
    viewer.run();

    exit(EXIT_SUCCESS);
#endif  // VOXEL_VIEWER_TESTING
    // ------------------------------------------------------------------------

  } catch (const std::exception& e) {
    std::cerr << "[Error] " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
