#ifndef COMPUTESHADER_HPP
#define COMPUTESHADER_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

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

        // Create shader object
        GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shader, 1, &shaderCode, nullptr);
        glCompileShader(shader);

        // Check for compilation errors
        GLint compileSuccess = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compileSuccess);
        if (!compileSuccess) {
            GLint logLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<GLchar> infoLog(logLength);
            glGetShaderInfoLog(shader, logLength, nullptr, infoLog.data());
            std::cerr << "Compute shader compilation failed:\n" << infoLog.data() << std::endl;
        }

        // Create program and attach shader
        ID = glCreateProgram();
        glAttachShader(ID, shader);
        glLinkProgram(ID);

        // Check for linking errors
        GLint linkSuccess = 0;
        glGetProgramiv(ID, GL_LINK_STATUS, &linkSuccess);
        if (!linkSuccess) {
            GLint logLength = 0;
            glGetProgramiv(ID, GL_INFO_LOG_LENGTH, &logLength);
            std::vector<GLchar> infoLog(logLength);
            glGetProgramInfoLog(ID, logLength, nullptr, infoLog.data());
            std::cerr << "Compute shader linking failed:\n" << infoLog.data() << std::endl;
        }

        // Shader object no longer needed after linking
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
