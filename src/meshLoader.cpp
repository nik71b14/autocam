#include "meshLoader.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>
#include <cfloat>
#include <algorithm>

float loadMesh(const char* path, std::vector<float>& vertices, std::vector<unsigned int>& indices) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals);
    if (!scene || !scene->HasMeshes()) throw std::runtime_error("Failed to load mesh");

    aiMesh* mesh = scene->mMeshes[0];

    // Initialize min/max extents
    glm::vec3 minExtents(FLT_MAX);
    glm::vec3 maxExtents(-FLT_MAX);

    // Step 1: Calculate the bounding box of the model
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        const aiVector3D& vertex = mesh->mVertices[i];
        minExtents.x = std::min(minExtents.x, vertex.x);
        minExtents.y = std::min(minExtents.y, vertex.y);
        minExtents.z = std::min(minExtents.z, vertex.z);

        maxExtents.x = std::max(maxExtents.x, vertex.x);
        maxExtents.y = std::max(maxExtents.y, vertex.y);
        maxExtents.z = std::max(maxExtents.z, vertex.z);
    }

    std::cout << "Min Extents: (" << minExtents.x << ", " << minExtents.y << ", " << minExtents.z << ")" << std::endl;
    std::cout << "Max Extents: (" << maxExtents.x << ", " << maxExtents.y << ", " << maxExtents.z << ")" << std::endl;

    glm::vec3 size = maxExtents - minExtents;

    std::cout << "Size: (" << size.x << ", " << size.y << ", " << size.z << ")" << std::endl;

    float scale = 1.0f / std::max(size.x, size.y); // Normalize to fit in a unit cube
    glm::vec3 center = (maxExtents + minExtents) / 2.0f;

    std::cout << "Scale factor: " << scale << std::endl;
    std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;

    // Step 2: Apply scaling and centering
    vertices.reserve(mesh->mNumVertices * 3);
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        aiVector3D& vertex = mesh->mVertices[i];

        vertex.x -= center.x;
        vertex.y -= center.y;
        vertex.z -= center.z;

        vertex *= scale;

        vertices.push_back(vertex.x);
        vertices.push_back(vertex.y);
        vertices.push_back(vertex.z);
    }

    // Step 3: Upload the indices
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace& face = mesh->mFaces[i];
        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);
    }

    float zSize = std::abs(maxExtents.z - minExtents.z) * scale;
    return zSize;
}