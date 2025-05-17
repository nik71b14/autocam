#pragma once

#include <glad/glad.h>
#include <iostream>
#include <glm/glm.hpp>
#include <vector>

// Logs the size of an SSBO in megabytes
int logSSBOSize(GLuint ssbo);

// Computes the axis-aligned bounding box of a list of 3D vertices.
// Input is a flat float array (x0, y0, z0, x1, y1, z1, ...).
// Returns a pair: {min, max} coordinates.
std::pair<glm::vec3, glm::vec3> computeBoundingBox(const std::vector<float>& vertices);
std::pair<glm::vec3, glm::vec3> computeBoundingBoxVec3(const std::vector<glm::vec3>& vertices);

void queryGPULimits();
