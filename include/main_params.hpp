#pragma once

// Debugging
// #define MAIN_DEBUG_OUTPUT

// Paths
// #define STL_PATH "models/bizzarro.stl"
// #define STL_PATH "models/punta.stl"
// #define STL_PATH "models/puntahex.stl"
// #define STL_PATH "models/puntahexshort.stl"
// #define STL_PATH "models/model2_bin.stl"
// #define STL_PATH "models/hemispheric_mill_10.stl"
// #define STL_PATH "models/point_mill_10.stl"
// #define STL_PATH "models/point_mill_10_2.stl"
// #define STL_PATH "models/step_mill_10.stl"
// #define STL_PATH "models/cyl_mill_12.stl"
// #define STL_PATH "models/workpiece_10_10_5.stl"
// #define STL_PATH "models/workpiece_100_100_50.stl"
// #define STL_PATH "models/cone_trunc.stl"
// #define STL_PATH "models/cube.stl"
// #define STL_PATH "models/workpiece_rotated.stl"
// #define STL_PATH "models/mill10.stl"
#define STL_PATH "models/cube100.stl"

// #define GCODE_PATH "gcode/star_pocket.gcode"
#define GCODE_PATH "gcode/pocket.gcode"

// Vozelization parameters
#define WHITE glm::vec3(1.0f, 1.0f, 1.0f)
#define MEM_512MB 512 * 1024 * 1024  // 512 MB memory budget for voxelization
#define MEM_1GB 1024 * 1024 * 1024   // 1 GB memory budget for voxelization
#define RESOLUTION 0.05              // Voxel resolution in object units (e.g., mm)

// CAM
#define SPEED_FACTOR 20.0  // Speed factor for simulation