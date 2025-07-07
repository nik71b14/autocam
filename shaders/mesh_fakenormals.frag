// MODEL WITH FAKE NORMALS
#version 460 core

in vec3 FakeNormal;
in vec3 FragPos;

uniform vec3 lightDir;
uniform vec4 uColor;

out vec4 FragColor;

void main() {
    vec3 norm = normalize(FakeNormal);
    vec3 light = normalize(-lightDir);

    float diff = max(dot(norm, light), 0.0);
    
    float ambient = 0.5; // ambient light intensity (0 = none, 1 = full color)
    vec3 color = (ambient + (1.0 - ambient) * diff) * uColor.rgb;

    FragColor = vec4(color, uColor.a);
}