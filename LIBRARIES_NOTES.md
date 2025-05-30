#include <iostream>
#include <vector>
#include <stdexcept>

// Parses the OpenGL specification and generates code (usually .c and .h files).
// When your program starts, you call gladLoadGL() (or gladLoadGLLoader() for
// GLFW/Vulkan). GLAD loads all available OpenGL function pointers. After that,
// you can just use OpenGL as if they were regular C functions. glad.h and glad.c
// were generated using https://glad.dav1d.de/, with settings: OpenGL, Core, 4.6,
// GLAD loader: glad, Language: C/C++, Extensions: GL_EXT_framebuffer_object,
// GL_EXT_texture_compression_s3tc, GL_EXT_texture_filter_anisotropic,
// GL_EXT_texture_sRGB_decode.
#include <glad/glad.h>

// GLFW handles all the platform-specific boilerplate for setting up an OpenGL
// rendering environment. Instead of writing native Windows/macOS/Linux code to
// open a window, set up an OpenGL context, and handle input events, GLFW does
// all that in a cross-platform way.
#include <GLFW/glfw3.h>

// OpenGL math library, installed from MSYS2 shell with:
// pacman -S mingw-w64-ucrt-x86_64-glm
// (headers-only library, files are installed in
// C:\msys64\ucrt64\include\glm)
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// A C++ library designed to import 3D model files, installed from the MSYS2
// shell with: pacman -S mingw-w64-ucrt-x86_64-assimp. Assimp headers are in:
// C:/msys64/ucrt64/include/assimp, and the library is in:
// C:/msys64/ucrt64/lib/libassimp.a.
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "shader.hpp"
#include "meshLoader.hpp"
#include "GLUtils.hpp"
#include "voxelViewer.hpp"
#include "voxelizer.hpp"
#include "voxelizerUtils.hpp"
