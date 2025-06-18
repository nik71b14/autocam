#pragma once

#include <string>
#include <glm/glm.hpp>

void analizeVoxelizedObject(const std::string& filename);
void subtract(const std::string& obj1Path, const std::string& obj2Path, glm::ivec3 offset);
