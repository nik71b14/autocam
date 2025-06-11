#pragma once

#include <vector>

struct Mesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

struct MeshWithNormals {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<float> normals; // Normals for each vertex
};
