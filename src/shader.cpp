#include "shader.hpp"

#include <glad/glad.h>

#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>
#include <string>

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
  std::string vertexCode, fragmentCode;
  std::ifstream vShaderFile(vertexPath), fShaderFile(fragmentPath);

  if (!vShaderFile.is_open() || !fShaderFile.is_open()) throw std::runtime_error("Failed to open shader files");

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

Shader::Shader(const char* computePath) {
  std::ifstream cShaderFile(computePath);
  if (!cShaderFile.is_open()) throw std::runtime_error("Failed to open compute shader file");

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

void Shader::use() const {
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

void Shader::dismiss() const {
  glUseProgram(0);
  //   GLenum err = glGetError();
  //   if (err != GL_NO_ERROR) {
  //     std::cerr << "OpenGL error after glUseProgram(0): " << std::hex << err << std::endl;
  //   }
}

void Shader::setBool(const std::string& name, bool value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), static_cast<int>(value)); }

void Shader::setUInt(const std::string& name, unsigned int value) const { glUniform1ui(glGetUniformLocation(ID, name.c_str()), value); }
void Shader::setMat4(const std::string& name, const glm::mat4& mat) const {
  glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}
void Shader::setFloat(const std::string& name, float value) const { glUniform1f(glGetUniformLocation(ID, name.c_str()), value); }
void Shader::setVec4(const std::string& name, const glm::vec4& value) const { glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }
void Shader::setInt(const std::string& name, int value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), value); }
void Shader::setLong(const std::string& name, long value) const { glUniform1i(glGetUniformLocation(ID, name.c_str()), static_cast<int>(value)); }
void Shader::setBufferBase(unsigned int bindingPoint, GLuint buffer) const { glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, buffer); }
void Shader::setVec2(const std::string& name, const glm::vec2& value) const { glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }
void Shader::setVec3(const std::string& name, const glm::vec3& value) const { glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }
void Shader::setIVec2(const std::string& name, const glm::ivec2& value) const { glUniform2iv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }
void Shader::setIVec3(const std::string& name, const glm::ivec3& value) const { glUniform3iv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]); }

unsigned int Shader::compile(GLenum type, const char* code) {
  unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &code, nullptr);
  glCompileShader(shader);

  int success;
  char infoLog[1024];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
    std::string typeStr = (type == GL_VERTEX_SHADER ? "VERTEX" : type == GL_FRAGMENT_SHADER ? "FRAGMENT" : type == GL_COMPUTE_SHADER ? "COMPUTE" : "UNKNOWN");
    std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << typeStr << "\n"
              << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
    throw std::runtime_error("Shader compilation failed.");
  }

  return shader;
}

void Shader::checkLinkErrors(unsigned int program) {
  int success;
  char infoLog[1024];
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(program, 1024, nullptr, infoLog);
    std::cerr << "ERROR::PROGRAM_LINKING_ERROR:\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
    throw std::runtime_error("Shader linking failed.");
  }
}