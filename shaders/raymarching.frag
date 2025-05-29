#version 460 core

out vec4 fragColor;

// SSBOs for transition data and prefix sum
layout(std430, binding = 0) readonly buffer TransitionBuffer {
    uint transitions[];
};

layout(std430, binding = 1) readonly buffer PrefixSumBuffer {
    uint prefixSums[];
};

// Uniforms
uniform ivec3 resolution;        // (RES_X, RES_Y, RES_Z)
uniform int maxTransitions;      // Maximum transitions per Z column
uniform mat4 invViewProj;        // Inverse of view-projection matrix
uniform vec3 cameraPos;
uniform ivec2 screenResolution;  // Screen size

bool isInsideVoxel(int x, int y, int z) {
    // Calculate column index in 2D grid
    int columnIndex = y * resolution.x + x;
    
    // Boundary check
    if (columnIndex < 0 || columnIndex >= resolution.x * resolution.y - 1) {
        return false;
    }
    
    uint start = prefixSums[columnIndex];
    uint end = prefixSums[columnIndex + 1];
    
    // Binary search would be better here for large numbers of transitions
    bool inside = false;
    for (uint i = start; i < end && i < transitions.length(); ++i) {
        uint transitionZ = transitions[i];
        if (z < int(transitionZ)) break;
        inside = !inside;
    }
    return inside;
}

void main() {
    // Reconstruct view ray from screen position
    vec2 uv = (gl_FragCoord.xy / vec2(screenResolution)) * 2.0 - 1.0;
    
    vec4 nearPoint = invViewProj * vec4(uv, 0.0, 1.0);
    vec4 farPoint  = invViewProj * vec4(uv, 1.0, 1.0);
    
    vec3 rayOrigin = nearPoint.xyz / nearPoint.w;
    vec3 rayEnd    = farPoint.xyz / farPoint.w;
    vec3 rayDir    = normalize(rayEnd - rayOrigin);
    
    // Volume bounds (assuming [-0.5, 0.5] cube)
    vec3 boxMin = vec3(-0.5);
    vec3 boxMax = vec3(0.5);
    
    // Ray-box intersection
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
    vec3 endPos = rayOrigin + rayDir * tmax;
    
    // Better step size calculation
    float voxelSize = 1.0 / max(resolution.x, max(resolution.y, resolution.z));
    float stepSize = voxelSize * 0.5; // Smaller steps for better accuracy
    
    // Normalized direction to voxel space conversion
    vec3 voxelSizeVec = vec3(resolution);
    vec3 invVoxelSize = 1.0 / voxelSizeVec;
    
    int maxSteps = int(length(boxMax - boxMin) / stepSize) * 2;
    
    for (int i = 0; i < maxSteps; ++i) {
        // Convert to voxel grid coordinates
        vec3 normalizedPos = (pos - boxMin) / (boxMax - boxMin);
        ivec3 ipos = ivec3(floor(normalizedPos * voxelSizeVec));
        
        // Check bounds
        if (all(greaterThanEqual(ipos, ivec3(0))) && 
            all(lessThan(ipos, resolution))) 
        {
            // if (isInsideVoxel(ipos.x, ipos.y, ipos.z)) {
            //     // Visualize with gradient based on Z position
            //     float t = float(ipos.z) / float(resolution.z);
            //     fragColor = vec4(t, 1.0 - t, 0.5, 1.0);
            //     return;
            // }
            if (isInsideVoxel(ipos.x, ipos.y, ipos.z)) {
              // BLUE GRADIENT: Front = light blue, Back = dark blue
              float depthFactor = float(ipos.z) / float(resolution.z);
              vec3 lightBlue = vec3(0.5, 0.7, 1.0);  // Bright sky blue
              vec3 darkBlue = vec3(0.0, 0.1, 0.3);   // Deep ocean blue
              vec3 color = mix(lightBlue, darkBlue, depthFactor);
              
              fragColor = vec4(color, 1.0);
              return;
            }
        }
        
        pos += rayDir * stepSize;
        if (distance(pos, rayOrigin) > distance(endPos, rayOrigin)) {
            break;
        }
    }
    
    // If we get here, we didn't hit anything solid
    fragColor = vec4(0.0, 0.0, 0.0, 1.0); // Black background
}


/*
// SUPER SIMPLE TEST SHADER
void main() {
    fragColor = vec4(1, 0, 0, 1); // <-- red everywhere
    return;
}
*/

/*
// A LITTLE MORE COMPLEX TEST SHADER
#version 460 core

out vec4 fragColor;

// SSBOs for transition data and prefix sum
layout(std430, binding = 0) buffer TransitionBuffer {
    uint transitions[];
};

layout(std430, binding = 1) buffer PrefixSumBuffer {
    uint prefixSums[];
};

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