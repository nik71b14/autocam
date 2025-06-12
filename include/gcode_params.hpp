#pragma once

// Debugging
#define DEBUG_OUTPUT true

// General
#define IDENTITY_MODEL glm::mat4(1.0f) // Identity matrix for model transformations

// Graphics
#define AXES_LENGTH 10.0f
#define SPHERE_STACKS 12
#define SPHERE_SLICES 24
#define SPHERE_RADIUS 1.0f
#define TOOL_STL_PATH "models/hemispheric_mill_10.stl" // Path to the tool STL file
#define WORKPIECE_STL_PATH "models/workpiece_100_100_50_neg.stl" // Path to the workpiece STL file

// Mouse interaction
#define INITIAL_YAW 180.0f // Initial yaw angle for camera
#define INITIAL_PITCH 0.0f // Initial pitch angle for camera
#define INITIAL_CAMERA_DISTANCE 50.0f
#define INITIAL_CAMERA_POSITION glm::vec3(0.0f, 0.0f, 50.0f) // Initial camera position
#define INITIAL_CAMERA_TARGET glm::vec3(0.0f) // Initial camera target position
#define INITIAL_VIEW_CENTER glm::vec2(0.0f) // Center of the orthographic view
#define INITIAL_VIEW_WIDTH 200.0f // Width of the orthographic view

// Lighting
#define LIGHT_DIRECTION glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)) // Direction of the light source

