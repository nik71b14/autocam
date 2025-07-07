// MODEL WITH FAKE NORMALS
#version 460 core

layout(location = 0) in vec3 aPos;

out vec3 FragPos;
out vec3 FakeNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    FragPos = vec3(worldPos);
    FakeNormal = normalize(mat3(uModel) * aPos); // approximate normal
    gl_Position = uProj * uView * worldPos;
}

