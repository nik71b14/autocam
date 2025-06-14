// THIS IS ABOUT OK
#version 460 core

out vec4 fragColor;

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
uniform vec3 color;
uniform float normalizedZSpan;  //%%% New uniform for Z span

bool isInsideVoxel(int x, int y, int z) {
    int columnIndex = y * resolution.x + x;
    
    if (columnIndex < 0 || columnIndex >= resolution.x * resolution.y - 1) {
        return false;
    }
    
    uint start = prefixSums[columnIndex];
    uint end = prefixSums[columnIndex + 1];
    
    bool inside = false;
    for (uint i = start; i < end && i < transitions.length(); ++i) {
        uint transitionZ = transitions[i];
        if (z < int(transitionZ)) break;
        inside = !inside;
    }
    return inside;
}

void main() {

    // fragColor = vec4(0.0, 1.0, 0.0, 1.0);
    // return;

    // Calculate normalized device coordinates (NDC) from fragment coordinates, i.e. in the range [-1, 1] both for x and y
    vec2 uv = (gl_FragCoord.xy / vec2(screenResolution)) * 2.0 - 1.0;
    // ec2 uv = vec2((gl_FragCoord.x / float(screenResolution.x)) * 2.0 - 1.0, (gl_FragCoord.y / float(screenResolution.y)) * 2.0 - 1.0);

    vec4 nearClip = vec4(uv, -1.0, 1.0);
    vec4 farClip  = vec4(uv,  1.0, 1.0);

    vec4 nearWorld = invViewProj * nearClip;
    vec4 farWorld  = invViewProj * farClip;

    nearWorld.xyz /= nearWorld.w;
    farWorld.xyz  /= farWorld.w;

    vec3 rayOrigin = nearWorld.xyz;
    vec3 rayDir = normalize(farWorld.xyz - nearWorld.xyz);

    // Calculate aspect ratio
    float aspect = float(screenResolution.x) / float(screenResolution.y);
    
    // Volume bounds (fixed cubic volume in world space)
    //vec3 boxMin = vec3(-0.5, -0.5, -0.5);
    //vec3 boxMax = vec3(0.5, 0.5, 0.5);
    float zHalf = normalizedZSpan * 0.5; //%%%%
    vec3 boxMin = vec3(-0.5, -0.5, -zHalf);
    vec3 boxMax = vec3(0.5, 0.5, zHalf);
    
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
    
    float voxelSize = 1.0 / max(resolution.x, max(resolution.y, resolution.z));
    float stepSize = voxelSize * 0.5;
    
    vec3 voxelSizeVec = vec3(resolution);
    
    int maxSteps = int(length(boxMax - boxMin) / stepSize) * 2;
    
    for (int i = 0; i < maxSteps; ++i) {
        // Convert to voxel grid coordinates
        // vec3 normalizedPos = (pos - boxMin) / (boxMax - boxMin);
        // ivec3 ipos = ivec3(floor(normalizedPos * voxelSizeVec));
        //%%%%%% Convert to voxel grid coordinates
        vec3 boxSize = boxMax - boxMin;
        vec3 normalizedPos = (pos - boxMin) / boxSize;
        ivec3 ipos = ivec3(floor(normalizedPos * vec3(resolution)));

        
        if (all(greaterThanEqual(ipos, ivec3(0))) && 
            all(lessThan(ipos, resolution))) 
        {
            if (isInsideVoxel(ipos.x, ipos.y, ipos.z)) {
                float depthFactor = float(ipos.z) / float(resolution.z);
                vec3 darkBlue = vec3(0.0, 0.1, 0.3);
                vec3 darkColor = color * 0.35;
                vec3 color = mix(color, darkColor, depthFactor);
                fragColor = vec4(color, 1.0);
                return;
            }
        }
        
        pos += rayDir * stepSize;
        if (distance(pos, rayOrigin) > distance(endPos, rayOrigin)) {
            break;
        }
    }
    
    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
}


/*
#version 460 core

out vec4 fragColor;

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

bool isInsideVoxel(int x, int y, int z) {
    int columnIndex = y * resolution.x + x;
    
    if (columnIndex < 0 || columnIndex >= resolution.x * resolution.y - 1) {
        return false;
    }
    
    uint start = prefixSums[columnIndex];
    uint end = prefixSums[columnIndex + 1];
    
    bool inside = false;
    for (uint i = start; i < end && i < transitions.length(); ++i) {
        uint transitionZ = transitions[i];
        if (z < int(transitionZ)) break;
        inside = !inside;
    }
    return inside;
}

void main() {
    vec2 uv = (gl_FragCoord.xy / vec2(screenResolution)) * 2.0 - 1.0;
    vec4 nearClip = vec4(uv, -1.0, 1.0);
    vec4 farClip  = vec4(uv,  1.0, 1.0);

    vec4 nearWorld = invViewProj * nearClip;
    vec4 farWorld  = invViewProj * farClip;

    nearWorld.xyz /= nearWorld.w;
    farWorld.xyz  /= farWorld.w;

    vec3 rayOrigin = nearWorld.xyz;
    vec3 rayDir = normalize(farWorld.xyz - nearWorld.xyz);

    // Calculate aspect ratio
    float aspect = float(screenResolution.x) / float(screenResolution.y);
    
    // Volume bounds (assuming [-0.5, 0.5] cube in YZ, adjusted in X for aspect ratio)
    vec3 boxMin = vec3(-0.5 * aspect, -0.5, -0.5);
    vec3 boxMax = vec3( 0.5 * aspect,  0.5,  0.5);
    
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
    float stepSize = voxelSize * 0.5;
    
    vec3 voxelSizeVec = vec3(resolution);
    
    int maxSteps = int(length(boxMax - boxMin) / stepSize) * 2;
    
    for (int i = 0; i < maxSteps; ++i) {
        // Convert to voxel grid coordinates
        vec3 normalizedPos = (pos - boxMin) / (boxMax - boxMin);
        ivec3 ipos = ivec3(floor(normalizedPos * voxelSizeVec));
        
        // Check bounds
        if (all(greaterThanEqual(ipos, ivec3(0))) && 
            all(lessThan(ipos, resolution))) 
        {
            if (isInsideVoxel(ipos.x, ipos.y, ipos.z)) {
                // BLUE GRADIENT: Front = light blue, Back = dark blue
                float depthFactor = float(ipos.z) / float(resolution.z);
                vec3 lightBlue = vec3(0.5, 0.7, 1.0);
                vec3 darkBlue = vec3(0.0, 0.1, 0.3);
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
    
    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
*/