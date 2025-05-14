// --- Mesh Loading ---
#pragma once

#include <vector>
#include <string>

// Forward declarations
namespace Assimp {
    class Importer;
}
struct aiScene;
struct aiMesh;

float loadMesh(const char* path, std::vector<float>& vertices, std::vector<unsigned int>& indices);
