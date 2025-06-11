#version 460 core

//uniform vec3 uColor;
uniform vec4 uColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(uColor);
}
