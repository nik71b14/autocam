#pragma once

#include <string>

#include <glad/glad.h> // This before GLFW to avoid conflicts
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class Shader {
public:
    unsigned int ID;

    // Vertex + Fragment shader constructor
    Shader(const char* vertexPath, const char* fragmentPath);

    // Compute shader constructor
    Shader(const char* computePath);

    void use() const;

    void setUInt(const std::string& name, unsigned int value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setIVec2(const std::string& name, const glm::ivec2& value) const;
    void setIVec3(const std::string& name, const glm::ivec3& value) const;
    void setBufferBase(unsigned int bindingPoint, GLuint buffer) const;

private:
    static unsigned int compile(GLenum type, const char* code);
    static void checkLinkErrors(unsigned int program);
};
