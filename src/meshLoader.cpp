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

    float scale = 1.0f / std::max(size.x, size.y); // Normalize
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

float loadMeshVec3(const char* path, std::vector<glm::vec3>& vertices, std::vector<unsigned int>& indices) {
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

    std::cout << "Min Extents: (" << minExtents.x << ", " << minExtents.y << ", " << minExtents.z << ")\n";
    std::cout << "Max Extents: (" << maxExtents.x << ", " << maxExtents.y << ", " << maxExtents.z << ")\n";

    glm::vec3 size = maxExtents - minExtents;
    std::cout << "Size: (" << size.x << ", " << size.y << ", " << size.z << ")\n";

    float scale = 1.0f / std::max(size.x, size.y); // Normalize
    glm::vec3 center = (maxExtents + minExtents) * 0.5f;

    std::cout << "Scale factor: " << scale << "\n";
    std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z << ")\n";

    // Step 2: Apply scaling and centering
    vertices.reserve(mesh->mNumVertices);
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        aiVector3D vertex = mesh->mVertices[i]; // copy

        vertex.x = (vertex.x - center.x) * scale;
        vertex.y = (vertex.y - center.y) * scale;
        vertex.z = (vertex.z - center.z) * scale;

        vertices.emplace_back(vertex.x, vertex.y, vertex.z);
    }

    // Step 3: Upload the indices
    indices.reserve(mesh->mNumFaces * 3);
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace& face = mesh->mFaces[i];
        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);
    }

    //@@@ Debug: Recalculate the bounding box after scaling and centering -----------
    glm::vec3 newMinExtents(FLT_MAX);
    glm::vec3 newMaxExtents(-FLT_MAX);

    for (const auto& vertex : vertices) {
        newMinExtents.x = std::min(newMinExtents.x, vertex.x);
        newMinExtents.y = std::min(newMinExtents.y, vertex.y);
        newMinExtents.z = std::min(newMinExtents.z, vertex.z);

        newMaxExtents.x = std::max(newMaxExtents.x, vertex.x);
        newMaxExtents.y = std::max(newMaxExtents.y, vertex.y);
        newMaxExtents.z = std::max(newMaxExtents.z, vertex.z);
    }

    std::cout << "New Min Extents: (" << newMinExtents.x << ", " << newMinExtents.y << ", " << newMinExtents.z << ")\n";
    std::cout << "New Max Extents: (" << newMaxExtents.x << ", " << newMaxExtents.y << ", " << newMaxExtents.z << ")\n";
    // -------------------------------------------------------------------------------
    
    float zSize = std::abs(maxExtents.z - minExtents.z) * scale;
    return zSize;
}
