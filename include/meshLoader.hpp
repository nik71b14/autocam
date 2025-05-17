// --- Mesh Loading ---
#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>

// Forward declarations
namespace Assimp {
    class Importer;
}
struct aiScene;
struct aiMesh;

float loadMesh(const char* path, std::vector<float>& vertices, std::vector<unsigned int>& indices);
float loadMeshVec3(const char* path, std::vector<glm::vec3>& vertices, std::vector<unsigned int>& indices);

