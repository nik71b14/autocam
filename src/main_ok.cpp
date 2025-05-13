#include <iostream>
#include <vector>
#include <stdexcept>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "shader.hpp"

const int RESOLUTION = 800;
const char* STL_PATH = "models/model3_bin.stl";
// const char* STL_PATH = "models/cube.stl";


GLFWwindow* window;
GLuint vao, vbo, ebo;
Shader* shader;

// --- Initialization ---
void setupGL() {
    if (!glfwInit()) throw std::runtime_error("Failed to init GLFW");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(RESOLUTION, RESOLUTION, "STL Viewer", NULL, NULL);
    if (!window) throw std::runtime_error("Failed to create GLFW window");
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        throw std::runtime_error("Failed to initialize GLAD");
}


// Load mesh with scaling and centering
void loadMesh(const char* path, std::vector<float>& vertices, std::vector<unsigned int>& indices) {
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

  float scale = 1.0f / glm::length(size); // Normalize to fit in a unit cube
  glm::vec3 center = (maxExtents + minExtents) / 2.0f;

  std::cout << "Scale factor: " << scale << std::endl;
  std::cout << "Center: (" << center.x << ", " << center.y << ", " << center.z << ")" << std::endl;

  // Step 2: Apply scaling and centering
  vertices.reserve(mesh->mNumVertices * 3);
  for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
      aiVector3D& vertex = mesh->mVertices[i];

      // Translate the vertex to center it
      vertex.x -= center.x;
      vertex.y -= center.y;
      vertex.z -= center.z;

      // Scale the vertex
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
}


void uploadMesh(const std::vector<float>& vertices, const std::vector<unsigned int>& indices) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

int main() {
    try {
        setupGL();

        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        loadMesh(STL_PATH, vertices, indices);

        std::cout << "Number of vertices: " << vertices.size() / 3 << std::endl;
        std::cout << "Number of faces: " << indices.size() / 3 << std::endl;

        uploadMesh(vertices, indices);

        shader = new Shader("shaders/vertex_ok.glsl", "shaders/fragment_ok.glsl");

        glEnable(GL_DEPTH_TEST);

        // Simple MVP setup
        glm::mat4 projection = glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 10.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 model = glm::mat4(1.0f);

        // Draw once
        glViewport(0, 0, RESOLUTION, RESOLUTION);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader->use();
        shader->setMat4("projection", projection);
        shader->setMat4("view", view);
        shader->setMat4("model", model);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glfwSwapBuffers(window);

        std::cout << "Rendered model. Press any key to exit.\n";

        // Wait for keypress
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                break;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    glfwTerminate();
    return EXIT_SUCCESS;
}
