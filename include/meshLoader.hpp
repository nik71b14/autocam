// --- Mesh Loading ---
#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "voxelizer.hpp"

// Forward declarations
namespace Assimp {
    class Importer;
}
struct aiScene;
struct aiMesh;

Mesh loadMesh(const char* path);

