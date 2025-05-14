#ifndef COMPUTESHADER_HPP
#define COMPUTESHADER_HPP

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class VoxelizerComputeShader {
public:
    GLuint ID;

    VoxelizerComputeShader(const std::string& path) {
        std::string code;
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open compute shader: " << path << std::endl;
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        code = buffer.str();

        const char* shaderCode = code.c_str();
        GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shader, 1, &shaderCode, nullptr);
        glCompileShader(shader);

        // Check compilation
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Compute shader compilation failed:\n" << infoLog << std::endl;
        }

        ID = glCreateProgram();
        glAttachShader(ID, shader);
        glLinkProgram(ID);

        // Check linking
        glGetProgramiv(ID, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(ID, 512, nullptr, infoLog);
            std::cerr << "Compute shader linking failed:\n" << infoLog << std::endl;
        }

        glDeleteShader(shader);
    }

    void use() const {
        glUseProgram(ID);
    }

    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3f(glGetUniformLocation(ID, name.c_str()), value.x, value.y, value.z);
    }

    void dispatch(GLuint x, GLuint y = 1, GLuint z = 1) const {
        glDispatchCompute(x, y, z);
    }

    void destroy() {
        glDeleteProgram(ID);
    }
};

#endif // COMPUTESHADER_HPP
