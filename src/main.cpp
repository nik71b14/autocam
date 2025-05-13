#include <iostream>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <thread>

// Parses the OpenGL specification and generates code (usually .c and .h files)
// When your program starts, you call gladLoadGL() (or gladLoadGLLoader() for GLFW/Vulkan)
// GLAD loads all available OpenGL function pointers
// After that, you can just use OpenGL as if they were regular C functions
// glad.h and glad.c generated using https://glad.dav1d.de/, with settings: OpenGL, Core, 4.6, GLAD loader: glad, Language: C/C++, Extensions: GL_EXT_framebuffer_object, GL_EXT_texture_compression_s3tc, GL_EXT_texture_filter_anisotropic, GL_EXT_texture_sRGB_decode
#include <glad/glad.h>

// GLFW takes care of all the platform-specific boilerplate for setting up an OpenGL rendering environment.
// Instead of writing native Windows/macOS/Linux code to open a window, set up an OpenGL context, and handle input events,
// GLFW does all that in a cross-platform way.
#include <GLFW/glfw3.h>

// OpenGL math library, installed from MSYS2 shell with: pacman -S mingw-w64-ucrt-x86_64-glm (headers-only lib, files are installed in C:\msys64\ucrt64\include\glm
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

//  It’s a C++ library designed to import 3D model files, installed from MSYS2 shell with: pacman -S mingw-w64-ucrt-x86_64-assimp (Assimp headers in: C:/msys64/ucrt64/include/assimp, library in: C:/msys64/ucrt64/lib/libassimp.a
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "shader.hpp"
#include "voxelVolume.hpp"
#include "quadtreeFlat.hpp"
#include "transitionCompressor.hpp"
#include "transitionCompressorGPU.hpp"

const int RESOLUTION = 1024;
const char* STL_PATH = "models/model3_bin.stl";
//const char* STL_PATH = "models/cone.stl";
// const char* STL_PATH = "models/cube.stl";


GLFWwindow* window;
GLuint fbo, colorTex, depthRbo;
GLuint vao, vbo, ebo;
Shader* shader;
VoxelVolume volume1;
QuadtreeVolume volume2(RESOLUTION, RESOLUTION);
TransitionCompressor transitionCompressor(RESOLUTION, RESOLUTION);
TransitionCompressorGPU transitionCompressorGPU(RESOLUTION, RESOLUTION); //###

// Compute shader
Shader* computeShader; //###

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


void createFramebuffer() {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RESOLUTION, RESOLUTION, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, RESOLUTION, RESOLUTION, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr); //###
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

    glGenRenderbuffers(1, &depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, RESOLUTION, RESOLUTION);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Framebuffer not complete.");
}

// --- Mesh Loading ---
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
  
    float scale = 1.0f / glm::length(std::max(size.x, size.y)); // Normalize to fit in a unit cube
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

        // std::cout << "Vertex: (" << vertex.x << ", " << vertex.y << ", " << vertex.z << ")" << std::endl;
  
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

    // Returns the scaled z size of the model
    float zSize = std::abs(maxExtents.z - minExtents.z) * scale;
    // std::cout << "Z size: " << zSize << std::endl;
    return zSize;
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

void sliceModel(int triangleCount, float zSpan, bool preview = false) {
    std::vector<uint8_t> pixelBuffer(RESOLUTION * RESOLUTION * 4);

    // glm::mat4 projection = glm::ortho(-0.51f, 0.51f, -0.51f, 0.51f, -10.0f, 10.0f);
    // glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    // glm::mat4 model = glm::mat4(1.0f);

    glm::mat4 projection = glm::ortho(-0.51f, 0.51f, -0.51f, 0.51f, 0.0f, zSpan); // Last two terms referred to eye position, are distances from the eye to the near and far planes
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, zSpan / 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 model = glm::mat4(1.0f);

    shader->use();
    shader->setMat4("projection", projection);
    shader->setMat4("view", view);
    shader->setMat4("model", model);

    for (int i = 0; i < RESOLUTION; ++i) {
        // float z = 0.5f - static_cast<float>(i) / RESOLUTION;

        float z = zSpan / 2.0f - static_cast<float>(i) * zSpan / RESOLUTION;

        glm::vec4 clippingPlane(glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)), z);
        shader->setVec4("clippingPlane", clippingPlane);

        // Render to offscreen framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, RESOLUTION, RESOLUTION);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_DEPTH_BUFFER_BIT);
        glDisable(GL_STENCIL_BUFFER_BIT); //@@@

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);

        // Read pixels from offscreen framebuffer
        glReadPixels(0, 0, RESOLUTION, RESOLUTION, GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer.data());

        // Compress pixelBuffer into a bit matrix
        std::vector<uint8_t> compressed((RESOLUTION * RESOLUTION + 7) / 8, 0);
        for (int j = 0; j < RESOLUTION * RESOLUTION; ++j) {
            int byteIndex = j / 8;
            int bitIndex = j % 8;
            if (pixelBuffer[j * 4] > 128)
                compressed[byteIndex] |= (1 << bitIndex);
        }

        // Compress pixelBuffer into a transitions array
        transitionCompressor.addSlice(pixelBuffer);

        if (i % (RESOLUTION / 10) == 0)
        std::cout << "Progress: " << (100 * i / RESOLUTION) << "%\n";

        //@@@ Optional preview on screen
        if (preview) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0); // Back to default framebuffer (window)
            glViewport(0, 0, RESOLUTION, RESOLUTION);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);

            glfwSwapBuffers(window);
            glfwPollEvents();
        }
    }

    std::cout << "Data size: " << volume2.getDataSize() / (1024.0 * 1024.0) << " MB\n";

    size_t compressedSize = static_cast<size_t>(RESOLUTION) * static_cast<size_t>(RESOLUTION) * static_cast<size_t>(RESOLUTION) / 8;
    std::cout << "Size of uint8_t: " << sizeof(uint8_t) << " bytes\n";
    size_t volume2Size = volume2.getDataSize();
    double savings = 100.0 * (static_cast<double>(compressedSize) - static_cast<double>(volume2Size)) / static_cast<double>(compressedSize);

    std::cout << "Bit compressed size: " << compressedSize / (1024.0 * 1024.0) << " MB\n";
    std::cout << "Quadtree compressed size: " << volume2Size / (1024.0 * 1024.0) << " MB\n";
    std::cout << "Memory savings: " << savings << " %\n";

    size_t transitionCompressedSize = transitionCompressor.getSize() * sizeof(uint32_t);
    std::cout << "Transition compressed size: " << transitionCompressedSize / (1024.0 * 1024.0) << " MB\n";
    double savings2 = 100.0 * (static_cast<double>(compressedSize) - static_cast<double>(transitionCompressedSize)) / static_cast<double>(compressedSize);
    std::cout << "Memory savings (transition): " << savings2 << " %\n";
    std::cout << "Transition compressor size: " << transitionCompressor.getSize() / (1024.0 * 1024.0) << " MB\n";
}

void sliceModel2(int triangleCount, float zSpan) {
    GLuint transitionSSBO, counterSSBO;
    const size_t MAX_TRANSITIONS = RESOLUTION * RESOLUTION * 2;

    glGenBuffers(1, &transitionSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_TRANSITIONS * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, transitionSSBO);

    glGenBuffers(1, &counterSSBO);
    uint32_t counterInit = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t), &counterInit, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, counterSSBO);

    computeShader->use();
    computeShader->setUInt("resolution", RESOLUTION);

    for (int i = 0; i < RESOLUTION; ++i) {
        float z = zSpan / 2.0f - static_cast<float>(i) * zSpan / RESOLUTION;
        glm::vec4 clippingPlane(glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)), z);

        shader->use();
        shader->setMat4("projection", glm::ortho(-0.51f, 0.51f, -0.51f, 0.51f, 0.0f, zSpan));
        shader->setMat4("view", glm::lookAt(glm::vec3(0, 0, zSpan / 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0)));
        shader->setMat4("model", glm::mat4(1.0f));
        shader->setVec4("clippingPlane", clippingPlane);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, RESOLUTION, RESOLUTION);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);

        // Bind framebuffer texture
        glBindImageTexture(0, colorTex, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);

        // Reset counter
        counterInit = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &counterInit);

        // Dispatch compute shader
        computeShader->use();
        glDispatchCompute(RESOLUTION, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

        // Read counter
        uint32_t count = 0;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, counterSSBO);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &count);

        // Read transitions
        std::vector<uint32_t> transitions(count);
        if (count > 0) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, transitionSSBO);
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count * sizeof(uint32_t), transitions.data());

            transitionCompressorGPU.addTransitionSlice(transitions);  // Implementa questo metodo
        }

        if (i % (RESOLUTION / 10) == 0)
            std::cout << "Progress: " << (100 * i / RESOLUTION) << "%\n";
    }

    //@@@ Performance Information Output
    size_t compressedSize = static_cast<size_t>(RESOLUTION) * static_cast<size_t>(RESOLUTION) * static_cast<size_t>(RESOLUTION) / 8;
    std::cout << "Size of uint8_t: " << sizeof(uint8_t) << " bytes\n";

    size_t volume2Size = volume2.getDataSize();
    double savings = 100.0 * (static_cast<double>(compressedSize) - static_cast<double>(volume2Size)) / static_cast<double>(compressedSize);

    std::cout << "Bit compressed size: " << compressedSize / (1024.0 * 1024.0) << " MB\n";
    std::cout << "Quadtree compressed size: " << volume2Size / (1024.0 * 1024.0) << " MB\n";
    std::cout << "Memory savings: " << savings << " %\n";

    size_t transitionCompressedSize = transitionCompressor.getSize() * sizeof(uint32_t);
    std::cout << "Transition compressed size: " << transitionCompressedSize / (1024.0 * 1024.0) << " MB\n";
    double savings2 = 100.0 * (static_cast<double>(compressedSize) - static_cast<double>(transitionCompressedSize)) / static_cast<double>(compressedSize);
    std::cout << "Memory savings (transition): " << savings2 << " %\n";
    std::cout << "Transition compressor size: " << transitionCompressorGPU.getSize() / (1024.0 * 1024.0) << " MB\n";

    glDeleteBuffers(1, &transitionSSBO);
    glDeleteBuffers(1, &counterSSBO);
}


/*
void sliceModel(int triangleCount) {
    std::vector<uint8_t> pixelBuffer(RESOLUTION * RESOLUTION * 4);

    // glm::mat4 projection = glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 10.0f);
    glm::mat4 projection = glm::ortho(-0.5f, 0.5f, -0.5f, 0.5f, -1.0f, 1.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
    glm::mat4 model = glm::mat4(1.0f);

    shader->use();
    shader->setMat4("projection", projection);
    shader->setMat4("view", view);
    shader->setMat4("model", model);

    for (int i = 0; i < RESOLUTION; ++i) {
        float z = 0.5f - static_cast<float>(i) / RESOLUTION;
        // shader->setFloat("clippingZ", z);
        //std::cout << "Z: " << z << std::endl;

        // Clipping plane
        glm::vec4 clippingPlane(glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)), z);
        shader->setVec4("clippingPlane", clippingPlane);

        // Set up framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, RESOLUTION, RESOLUTION);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);

        // Draw the mesh
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, triangleCount, GL_UNSIGNED_INT, 0);

        // Read pixels from the framebuffer
        glReadPixels(0, 0, RESOLUTION, RESOLUTION, GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer.data());

        // Compress the pixel buffer into a bit matrix
        std::vector<uint8_t> compressed((RESOLUTION * RESOLUTION + 7) / 8, 0);
        for (int j = 0; j < RESOLUTION * RESOLUTION; ++j) {
            int byteIndex = j / 8;
            int bitIndex = j % 8;
            if (pixelBuffer[j * 4] > 128)
                compressed[byteIndex] |= (1 << bitIndex);
        }

        // This is where you'd add your slice if you wanted to store it for later:
        // volume1.addSlice(compressed, RESOLUTION);
        //volume2.addSlice(z, compressed);

        // Transition compressor
        compressor.addSlice(pixelBuffer);

        if (i % (RESOLUTION / 10) == 0)
            std::cout << "Progress: " << (100 * i / RESOLUTION) << "%\n";

        // Now swap buffers to show the current slice
        glfwSwapBuffers(window);
        glfwPollEvents(); // Handle events (e.g., closing the window, key presses)

        // Slow down the loop to make the slicing process visible
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    //volume2.finalize();

    // Print slice offsets if needed for debugging
    // const auto& sliceOffsets = volume2.getSliceOffsets();
    // std::cout << "Slice offsets:\n";
    // for (size_t i = 0; i < sliceOffsets.size(); ++i) {
    //     std::cout << "Slice " << i << ": " << sliceOffsets[i] << "\n";
    // }
    std::cout << "Data size: " << volume2.getDataSize() / (1024.0 * 1024.0) << " MB\n";

    // Calculate memory savings for quadtree compression strategy
    size_t compressedSize = static_cast<size_t>(RESOLUTION) * static_cast<size_t>(RESOLUTION) * static_cast<size_t>(RESOLUTION) / 8;
    
    std::cout << "Size of uint8_t: " << sizeof(uint8_t) << " bytes\n";
    size_t volume2Size = volume2.getDataSize();

    double savings = 100.0 * (static_cast<double>(compressedSize) - static_cast<double>(volume2Size)) / static_cast<double>(compressedSize);

    std::cout << "Bit compressed size: " << compressedSize / (1024.0 * 1024.0) << " MB\n";
    std::cout << "Quadtree compressed size: " << volume2Size / (1024.0 * 1024.0) << " MB\n";
    std::cout << "Memory savings: " << savings << " %\n";

    // Calculate memory savings for transition compression strategy
    size_t transitionCompressedSize = compressor.getSize() * sizeof(uint32_t);
    std::cout << "Transition compressed size: " << transitionCompressedSize / (1024.0 * 1024.0) << " MB\n";
    double savings2 = 100.0 * (static_cast<double>(compressedSize) - static_cast<double>(transitionCompressedSize)) / static_cast<double>(compressedSize);
    std::cout << "Memory savings (transition): " << savings2 << " %\n";
    std::cout << "Transition compressor size: " << compressor.getSize() / (1024.0 * 1024.0) << " MB\n";
    
}
*/

int main() {

    try {
        setupGL();

        std::vector<float> vertices;
        std::vector<unsigned int> indices;
        float zSpan = 1.01f * loadMesh(STL_PATH, vertices, indices);

        std::cout << "Number of vertices: " << vertices.size() / 3 << std::endl;
        std::cout << "Number of faces: " << indices.size() / 3 << std::endl;

        uploadMesh(vertices, indices);

        shader = new Shader("shaders/vertex_ok.glsl", "shaders/fragment_ok.glsl");
        computeShader = new Shader("shaders/transition_compress.comp"); //###

        createFramebuffer();

        /*
        // ---------------------------------------------------------------
        //@@@ DEBUG: static rendering for testing
        glm::mat4 projection = glm::ortho(-0.51f, 0.51f, -0.51f, 0.51f, 0.0f, zSpan); // Last two terms referred to eye position, are distances from the eye to the near and far planes

        glm::mat4 view = glm::lookAt(glm::vec3(0, 0, zSpan / 2.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
        glm::mat4 model = glm::mat4(1.0f);
    
        shader->use();
        shader->setMat4("projection", projection);
        shader->setMat4("view", view);
        shader->setMat4("model", model);

        glm::vec4 clippingPlane(glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)), 0.0f);
        shader->setVec4("clippingPlane", clippingPlane);

        // Set up framebuffer
        // glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, RESOLUTION, RESOLUTION);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glEnable(GL_DEPTH_TEST);

        // Draw the mesh
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);

        glfwPollEvents();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return EXIT_SUCCESS;
        // ---------------------------------------------------------------
        */

        auto startTime = std::chrono::high_resolution_clock::now();

        // Slice using the CPU (CPU)
        sliceModel(indices.size(), zSpan, true);

        // Slice using a compute shader (GPU)
        //sliceModel2(indices.size(), zSpan);

        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsedTime = endTime - startTime;
        std::cout << "Execution time: " << elapsedTime.count() << " seconds\n";

        // Clean up
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
        glDeleteTextures(1, &colorTex);
        glDeleteRenderbuffers(1, &depthRbo);
        glDeleteFramebuffers(1, &fbo);
        delete shader;
        glfwDestroyWindow(window);
        glfwTerminate();

    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    glfwTerminate();
    return EXIT_SUCCESS;

}
