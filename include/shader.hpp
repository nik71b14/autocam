#ifndef SHADER_H
#define SHADER_H

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class Shader {
public:
    unsigned int ID;

    // Vertex and fragment shader constructor
    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vertexCode, fragmentCode;
        std::ifstream vShaderFile(vertexPath), fShaderFile(fragmentPath);

        if (!vShaderFile.is_open() || !fShaderFile.is_open())
            throw std::runtime_error("Failed to open shader files");

        std::stringstream vShaderStream, fShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();

        unsigned int vertex = compile(GL_VERTEX_SHADER, vertexCode.c_str());
        unsigned int fragment = compile(GL_FRAGMENT_SHADER, fragmentCode.c_str());

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);

        checkLinkErrors(ID);

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    // Compute shader constructor
    Shader(const char* computePath) {
        std::ifstream cShaderFile(computePath);
        if (!cShaderFile.is_open())
            throw std::runtime_error("Failed to open compute shader file");

        std::stringstream cShaderStream;
        cShaderStream << cShaderFile.rdbuf();
        std::string computeCode = cShaderStream.str();

        unsigned int compute = compile(GL_COMPUTE_SHADER, computeCode.c_str());

        ID = glCreateProgram();
        glAttachShader(ID, compute);
        glLinkProgram(ID);

        checkLinkErrors(ID);
        glDeleteShader(compute);
    }


    void use() const {
        //std::cout << "Shader Program ID: " << ID << std::endl;
        if (ID == 0) {
            std::cerr << "Shader program ID is 0, cannot use shader." << std::endl;
            return;
        }
        glUseProgram(ID);

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL error after glUseProgram: " << std::hex << err << std::endl;
        }
    }

    void setUInt(const std::string& name, unsigned int value) const {
        glUniform1ui(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setMat4(const std::string &name, const glm::mat4 &mat) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
    }

    void setFloat(const std::string &name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setVec4(const std::string& name, const glm::vec4& value) const {
        glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }

    void setInt(const std::string &name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }

    void setBufferBase(unsigned int bindingPoint, GLuint buffer) const {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, buffer);
    }

    void setVec2(const std::string& name, const glm::vec2& value) const {
        glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }

    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }

    void setIVec2(const std::string& name, const glm::ivec2& value) const {
        glUniform2iv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }    

    void setIVec3(const std::string& name, const glm::ivec3& value) const {
        glUniform3iv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
    }


private:
    // static unsigned int compile(GLenum type, const char* code) {
    //     unsigned int shader = glCreateShader(type);
    //     glShaderSource(shader, 1, &code, nullptr);
    //     glCompileShader(shader);

    //     int success;
    //     char infoLog[512];
    //     glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    //     if (!success) {
    //         glGetShaderInfoLog(shader, 512, nullptr, infoLog);
    //         throw std::runtime_error(std::string("Shader compilation error:\n") + infoLog);
    //     }

    //     return shader;
    // }
    static unsigned int compile(GLenum type, const char* code) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &code, nullptr);
        glCompileShader(shader);
    
        int success;
        char infoLog[1024];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            std::string typeStr = (type == GL_VERTEX_SHADER ? "VERTEX" :
                                   type == GL_FRAGMENT_SHADER ? "FRAGMENT" :
                                   type == GL_COMPUTE_SHADER ? "COMPUTE" : "UNKNOWN");
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << typeStr << "\n"
                      << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            throw std::runtime_error("Shader compilation failed.");
        }
    
        return shader;
    }

    // static void checkLinkErrors(unsigned int program) {
    //     int success;
    //     char infoLog[512];
    //     glGetProgramiv(program, GL_LINK_STATUS, &success);
    //     if (!success) {
    //         glGetProgramInfoLog(program, 512, nullptr, infoLog);
    //         throw std::runtime_error(std::string("Shader linking error:\n") + infoLog);
    //     }
    // }
    static void checkLinkErrors(unsigned int program) {
        int success;
        char infoLog[1024];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 1024, nullptr, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR:\n"
                      << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            throw std::runtime_error("Shader linking failed.");
        }
    }
};

#endif
