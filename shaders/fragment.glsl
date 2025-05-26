#version 460 core

in vec3 fragPosition;

uniform vec4 clippingPlane; // vec4(normal.xyz, -distance)

out vec4 FragColor;

void main() {
    // Clipping test: discard if on the "wrong" side of the plane
    if (dot(clippingPlane.xyz, fragPosition) + clippingPlane.w < 0.0)
        discard;

    // Front-facing color logic
    if (gl_FrontFacing) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0); // Outside (black)
    } else {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Inside (red)
    }
}
