#ifndef VOXELIZER_HPP
#define VOXELIZER_HPP

#include <GLFW/glfw3.h>
#include "shader.hpp"
#include "GLUtils.hpp"

void voxelize(
    const MeshBuffers& mesh,
    int triangleCount,
    float zSpan,
    Shader* shader,
    Shader* transitionShader,
    GLFWwindow* window,
    GLuint fbo,
    GLuint colorTex,
    int resolution,
    bool preview = false
);

#endif // VOXELIZER_HPP
