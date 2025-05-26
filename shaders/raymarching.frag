#version 460 core

uniform usamplerBuffer transitionBuffer;
uniform usamplerBuffer prefixSumBuffer;

uniform ivec3 resolution;      // (RES_X, RES_Y, RES_Z)
uniform int maxTransitions;    // Maximum transitions per column
uniform mat4 invViewProj;      // Inverse of view-projection matrix
uniform vec3 cameraPos;

out vec4 fragColor;

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
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(transitionBuffer, 0).x, textureSize(transitionBuffer, 0).y);
    uv = uv * 2.0 - 1.0; // to NDC

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
