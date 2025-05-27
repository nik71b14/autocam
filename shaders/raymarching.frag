#version 460

layout(std430, binding = 0) readonly buffer TransitionBuffer {
    uint transitions[];
};

layout(std430, binding = 1) readonly buffer PrefixSumBuffer {
    uint prefixSums[];
};

uniform ivec3 resolution;
uniform int maxTransitions;
uniform mat4 invViewProj;
uniform vec3 cameraPos;
uniform ivec2 screenResolution;

out vec4 fragColor;

bool isInsideVoxel(int x, int y, int z) {
    return false;
}

void main() {
    // Your raymarching code here...
    fragColor = vec4(1.0, 0.0, 0.0, 1.0); // For test only
}


/*
#version 460 core

out vec4 fragColor;

// SSBOs for transition data and prefix sum
layout(std430, binding = 0) buffer TransitionBuffer {
    uint transitions[];
};

layout(std430, binding = 1) buffer PrefixSumBuffer {
    uint prefixSums[];
};

// Uniforms
uniform ivec3 resolution;        // (RES_X, RES_Y, RES_Z)
uniform int maxTransitions;      // Unused here but kept if needed later
uniform mat4 invViewProj;        // Inverse of view-projection matrix
uniform vec3 cameraPos;
uniform ivec2 screenResolution;  // Screen size

// Raymarching helper: inside/outside test

bool isInsideVoxel(int x, int y, int z) {
    int columnIndex = y * resolution.x + x;
    uint start = prefixSums[columnIndex];
    uint end = prefixSums[columnIndex + 1];

    bool inside = false;
    for (uint i = start; i < end; ++i) {
        uint transitionZ = transitions[i];
        if (z < int(transitionZ)) break;
        inside = !inside;
    }
    return inside;
}

// bool isInsideVoxel(int x, int y, int z) {
//     return false;
// }

void main() {

    // Reconstruct view ray from screen position
    vec2 uv = (gl_FragCoord.xy / vec2(screenResolution)) * 2.0 - 1.0;

    vec4 nearPoint = invViewProj * vec4(uv, 0.0, 1.0);
    vec4 farPoint  = invViewProj * vec4(uv, 1.0, 1.0);

    vec3 rayOrigin = nearPoint.xyz / nearPoint.w;
    vec3 rayEnd    = farPoint.xyz / farPoint.w;
    vec3 rayDir    = normalize(rayEnd - rayOrigin);

    vec3 boxMin = vec3(0.0);
    vec3 boxMax = vec3(resolution);

    vec3 invDir = 1.0 / rayDir;
    vec3 t0s = (boxMin - rayOrigin) * invDir;
    vec3 t1s = (boxMax - rayOrigin) * invDir;

    vec3 tsmaller = min(t0s, t1s);
    vec3 tbigger = max(t0s, t1s);

    float tmin = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    float tmax = min(min(tbigger.x, tbigger.y), tbigger.z);

    if (tmin > tmax || tmax < 0.0) {
        discard;
    }

    tmin = max(tmin, 0.0);
    vec3 pos = rayOrigin + rayDir * tmin;

    const float stepSize = 1.0;
    int maxSteps = int(length(boxMax - boxMin) / stepSize);

    for (int i = 0; i < maxSteps; ++i) {
        ivec3 ipos = ivec3(floor(pos));
        if (all(greaterThanEqual(ipos, ivec3(0))) && all(lessThan(ipos, resolution))) {
            if (!isInsideVoxel(ipos.x, ipos.y, ipos.z)) {
                fragColor = vec4(1.0, 0.0, 0.0, 1.0); // Outside = red
                return;
            }
        }
        pos += rayDir * stepSize;
    }

    discard; // Nothing hit
}
*/

/*
void main() {
    fragColor = vec4(1, 0, 0, 1); // <-- red everywhere
    return;
}
*/

/*
uniform mat4 invViewProj;
uniform vec3 cameraPos;
uniform ivec2 screenResolution;

void main() {
    // Normalized screen coordinates [-1, 1]
    vec2 uv = (gl_FragCoord.xy / vec2(screenResolution)) * 2.0 - 1.0;

    // Compute ray origin and direction in world space
    vec4 nearPoint = invViewProj * vec4(uv, 0.0, 1.0);
    vec4 farPoint  = invViewProj * vec4(uv, 1.0, 1.0);
    
    vec3 rayOrigin = nearPoint.xyz / nearPoint.w;
    vec3 rayEnd    = farPoint.xyz / farPoint.w;
    vec3 rayDir    = normalize(rayEnd - rayOrigin);

    // Axis-aligned bounding box of unit cube
    vec3 boxMin = vec3(0.0);
    vec3 boxMax = vec3(1.0);

    vec3 invDir = 1.0 / rayDir;
    vec3 t0s = (boxMin - rayOrigin) * invDir;
    vec3 t1s = (boxMax - rayOrigin) * invDir;
    vec3 tMin = min(t0s, t1s);
    vec3 tMax = max(t0s, t1s);

    float tEnter = max(max(tMin.x, tMin.y), tMin.z);
    float tExit = min(min(tMax.x, tMax.y), tMax.z);

    if (tEnter <= tExit && tExit > 0.0) {
        // Ray intersects cube
        fragColor = vec4(1.0, 0.8, 0.2, 1.0); // bright yellowish color
    } else {
        // Simple background gradient
        float gradient = gl_FragCoord.y / float(screenResolution.y);
        fragColor = vec4(vec3(gradient), 1.0);
    }
}
*/

/*
uniform usamplerBuffer transitionBuffer;
uniform usamplerBuffer prefixSumBuffer;

uniform ivec3 resolution;      // (RES_X, RES_Y, RES_Z)
uniform int maxTransitions;    // Maximum transitions per column
uniform mat4 invViewProj;      // Inverse of view-projection matrix
uniform vec3 cameraPos;
uniform ivec2 screenResolution; // Add this to your uniforms

bool isInsideVoxel(int x, int y, int z) {
    int columnIndex = y * resolution.x + x;
    uint start = texelFetch(prefixSumBuffer, columnIndex).r;
    uint end = texelFetch(prefixSumBuffer, columnIndex + 1).r;

    bool inside = false;
    for (uint i = start; i < end; ++i) {
        uint transitionZ = texelFetch(transitionBuffer, int(i)).r;
        if (z < int(transitionZ)) break;
        inside = !inside;
    }
    return inside;
}

void main() {
    // Generate a ray from camera through pixel
    vec2 uv = (gl_FragCoord.xy / vec2(screenResolution)) * 2.0 - 1.0;

    vec4 nearPoint = invViewProj * vec4(uv, 0.0, 1.0);
    vec4 farPoint  = invViewProj * vec4(uv, 1.0, 1.0);

    vec3 rayOrigin = nearPoint.xyz / nearPoint.w;
    vec3 rayEnd    = farPoint.xyz / farPoint.w;
    vec3 rayDir    = normalize(rayEnd - rayOrigin);

    vec3 boxMin = vec3(0.0);
    vec3 boxMax = vec3(resolution);

    float tmin = 0.0;
    float tmax = 0.0;

    vec3 invDir = 1.0 / rayDir;
    vec3 t0s = (boxMin - rayOrigin) * invDir;
    vec3 t1s = (boxMax - rayOrigin) * invDir;
    vec3 tsmaller = min(t0s, t1s);
    vec3 tbigger = max(t0s, t1s);

    tmin = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    tmax = min(min(tbigger.x, tbigger.y), tbigger.z);

    if (tmin > tmax || tmax < 0.0) {
        discard;
    }

    tmin = max(tmin, 0.0);
    vec3 pos = rayOrigin + rayDir * tmin;

    const float stepSize = 1.0;
    int maxSteps = int(length(boxMax - boxMin) / stepSize);

    for (int i = 0; i < maxSteps; ++i) {
        ivec3 ipos = ivec3(floor(pos));
        if (all(greaterThanEqual(ipos, ivec3(0))) && all(lessThan(ipos, resolution))) {
            if (!isInsideVoxel(ipos.x, ipos.y, ipos.z)) {
                fragColor = vec4(0.5, 0.5, 0.5, 1.0); // Outside = gray
                return;
            }
        }
        pos += rayDir * stepSize;
    }

    discard; // Ray didn't hit anything outside
}
*/