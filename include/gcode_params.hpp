#pragma once

// Debugging
// #define DEBUG_OUTPUT_GCODE true

// General
#define IDENTITY_MODEL glm::mat4(1.0f)  // Identity matrix for model transformations

// Graphics
#define AXES_LENGTH 10.0f
#define SPHERE_STACKS 12
#define SPHERE_SLICES 24
#define SPHERE_RADIUS 1.0f
#define TOOL_STL_PATH "models/hemispheric_mill_10.stl"        // Path to the tool STL file
#define WORKPIECE_STL_PATH "models/workpiece_100_100_50.stl"  // Path to the workpiece STL file
#define WORKPIECE_COLOR glm::vec4(0.2f, 0.82f, 1.0f, 0.75f)   // Base color for the workpiece
#define TOOL_COLOR glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)          // Color for the tool

// Mouse interaction
#define INITIAL_WINDOW_WIDTH 1200  // Initial window width
#define INITIAL_WINDOW_HEIGHT 800  // Initial window height
#define INITIAL_YAW 180.0f         // Initial yaw angle for camera
#define INITIAL_PITCH 0.0f         // Initial pitch angle for camera
#define INITIAL_CAMERA_DISTANCE 50.0f
#define INITIAL_CAMERA_POSITION glm::vec3(0.0f, 0.0f, 50.0f)  // Initial camera position
#define INITIAL_CAMERA_TARGET glm::vec3(0.0f)                 // Initial camera target position
#define INITIAL_VIEW_CENTER glm::vec2(0.0f)                   // Center of the orthographic view
#define INITIAL_VIEW_WIDTH 200.0f                             // Width of the orthographic view

// Lighting
#define LIGHT_DIRECTION glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f))  // Direction of the light source

// Voxelized object
// #define VOXELIZED_WORKPIECE_PATH "test/workpiece_100_100_50.bin" // Path to the voxelized workpiece
#define VOXELIZED_WORKPIECE_PATH "test/workpiece_100_100_50.bin"  // Path to the voxelized workpiece
#define VOXELIZED_TOOL_PATH "test/hemispheric_mill_10.bin"        // Path to the voxelized tool
