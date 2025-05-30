#include "meshLoader.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <iostream>
#include <stdexcept>
#include <cfloat>
#include <algorithm>
#include "voxelizer.hpp"

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
