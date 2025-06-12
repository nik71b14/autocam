// TRUE NORMALS
#version 460 core

in vec3 Normal;
in vec3 FragPos;
// in vec3 FakeNormal; //&&&

uniform vec3 lightDir;
uniform vec4 uColor;

out vec4 FragColor;

void main() {
    // vec3 norm = normalize(Normal);
    // float diffuse = max(dot(norm, -lightDir), 0.0);
    // vec3 color = uColor.rgb * diffuse + 0.1 * uColor.rgb;  // Add some ambient
    // FragColor = vec4(color, uColor.a);

    // Hemisphere-style light
    vec3 norm = normalize(Normal);
    vec3 hemiLightDir = vec3(0.0, 1.0, 0.0); // from "above"
    float hemi = 0.5 * (dot(norm, hemiLightDir) + 1.0); // range [0,1]
    vec3 ambientColor = uColor.rgb * (0.3 + 0.2 * hemi);
    vec3 diffuseColor = uColor.rgb * 0.5 * max(dot(norm, -lightDir), 0.0);
    vec3 finalColor = ambientColor + diffuseColor;
    FragColor = vec4(finalColor, uColor.a);
    // FragColor = vec4(finalColor, 0.5f);
}

// MODEL WITHOUT NORMALS
/* #version 460 core

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
} */


/* #version 460 core

uniform vec4 uColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(uColor);
} */
