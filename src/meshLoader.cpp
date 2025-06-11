#include "meshLoader.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>
#include <cfloat>
#include <algorithm>
#include "meshTypes.hpp"
#include <glm/glm.hpp>

Mesh loadMesh(const char* path) {
  Assimp::Importer importer;
  const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals);
  if (!scene || !scene->HasMeshes()) throw std::runtime_error("Failed to load mesh");

  aiMesh* mesh = scene->mMeshes[0];

  // Extract vertices and indices from the mesh
  std::vector<float> vertices;
  vertices.reserve(mesh->mNumVertices * 3);
  for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
      aiVector3D& vertex = mesh->mVertices[i];

      vertices.push_back(vertex.x);
      vertices.push_back(vertex.y);
      vertices.push_back(vertex.z);
  }

  std::vector<unsigned int> indices;
  for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
      aiFace& face = mesh->mFaces[i];
      indices.push_back(face.mIndices[0]);
      indices.push_back(face.mIndices[1]);
      indices.push_back(face.mIndices[2]);
  }

  Mesh result;
  result.vertices = std::move(vertices);
  result.indices = std::move(indices);
  return result;
}

MeshWithNormals loadMeshWithNormals(const char* path) {
  Assimp::Importer importer;
  const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals);
  if (!scene || !scene->HasMeshes()) throw std::runtime_error("Failed to load mesh");

  aiMesh* mesh = scene->mMeshes[0];

  // Extract vertices and indices from the mesh
  std::vector<float> vertices;
  vertices.reserve(mesh->mNumVertices * 6);
  for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
      aiVector3D& vertex = mesh->mVertices[i];

      vertices.push_back(vertex.x);
      vertices.push_back(vertex.y);
      vertices.push_back(vertex.z);
      vertices.push_back(0); // Placeholder for normals
      vertices.push_back(0); // Placeholder for normals
      vertices.push_back(0); // Placeholder for normals
  }

  std::vector<unsigned int> indices;
  for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
      aiFace& face = mesh->mFaces[i];
      indices.push_back(face.mIndices[0]);
      indices.push_back(face.mIndices[1]);
      indices.push_back(face.mIndices[2]);
  }

  // Assign face normals to each vertex of each triangle
  for (size_t i = 0; i < indices.size(); i += 3) {
      unsigned int idx0 = indices[i];
      unsigned int idx1 = indices[i + 1];
      unsigned int idx2 = indices[i + 2];

      glm::vec3 v0(vertices[idx0 * 6 + 0], vertices[idx0 * 6 + 1], vertices[idx0 * 6 + 2]);
      glm::vec3 v1(vertices[idx1 * 6 + 0], vertices[idx1 * 6 + 1], vertices[idx1 * 6 + 2]);
      glm::vec3 v2(vertices[idx2 * 6 + 0], vertices[idx2 * 6 + 1], vertices[idx2 * 6 + 2]);

      glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

      // Set normal for v0
      for (const auto& idx : {idx0, idx1, idx2}) {
          vertices[idx * 6 + 3] = normal.x;
          vertices[idx * 6 + 4] = normal.y;
          vertices[idx * 6 + 5] = normal.z;
      }
  }

  //@@@ DEBUG: Print first 10 vertices with normals
  for (size_t i = 0; i < std::min(vertices.size() / 6, size_t(10)); ++i) {
    std::cout << "Vertex " << i << ": ("
          << vertices[i * 6 + 0] << ", "
          << vertices[i * 6 + 1] << ", "
          << vertices[i * 6 + 2] << ")  Normal: ("
          << vertices[i * 6 + 3] << ", "
          << vertices[i * 6 + 4] << ", "
          << vertices[i * 6 + 5] << ")\n";
  }

  MeshWithNormals result;
  result.vertices = std::move(vertices);
  result.indices = std::move(indices);
  return result;
}
