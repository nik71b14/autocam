#version 460 core

out vec4 fragColor;

layout(std430, binding = 0) readonly buffer TransitionBuffer { uint transitions[]; };

layout(std430, binding = 1) readonly buffer PrefixSumBuffer { uint prefixSums[]; };

uniform ivec3 resolution;
uniform int maxTransitions;
uniform mat4 invViewProj;
uniform vec3 cameraPos;
uniform ivec2 screenResolution;
uniform vec3 color;
uniform float normalizedZSpan;

// uniform vec3 lightDir1;
// uniform vec3 lightColor1;
// uniform vec3 lightDir2;
// uniform vec3 lightColor2;

bool isInsideVoxel(int x, int y, int z) {
  if (x < 0 || y < 0 || z < 0 || x >= resolution.x || y >= resolution.y || z >= resolution.z) {
    return false;
  }

  int columnIndex = y * resolution.x + x;
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

vec3 computeSmoothedNormal(ivec3 ipos) {
  vec3 n = vec3(0.0);

  ivec3 offsets[6] = ivec3[](
    ivec3(1, 0, 0), ivec3(-1, 0, 0), ivec3(0, 1, 0), ivec3(0, -1, 0),
    ivec3(0, 0, 1), ivec3(0, 0, -1)
  );

  for (int i = 0; i < 6; ++i) {
    ivec3 neighbor = ipos + offsets[i];
    if (!isInsideVoxel(neighbor.x, neighbor.y, neighbor.z)) {
      n += vec3(offsets[i]);
    }
  }

  ivec3 edgeOffsets[12] = ivec3[](
    ivec3(1, 1, 0), ivec3(-1, 1, 0), ivec3(1, -1, 0), ivec3(-1, -1, 0),
    ivec3(1, 0, 1), ivec3(-1, 0, 1), ivec3(1, 0, -1), ivec3(-1, 0, -1),
    ivec3(0, 1, 1), ivec3(0, -1, 1), ivec3(0, 1, -1), ivec3(0, -1, -1)
  );

  for (int i = 0; i < 12; ++i) {
    ivec3 neighbor = ipos + edgeOffsets[i];
    if (!isInsideVoxel(neighbor.x, neighbor.y, neighbor.z)) {
      n += 0.5 * normalize(vec3(edgeOffsets[i]));
    }
  }

  return normalize(n);
}

void main() {
  //@@@ Hardcoded light for debug --------------------------------
  vec3 lightDir1 = normalize(vec3(0.5, 1.0, 0.8));  // main light
  vec3 lightColor1 = vec3(1.0, 0.95, 0.8);          // warm sunlight

  vec3 lightDir2 = normalize(vec3(-0.3, 0.5, -0.7));  // fill light
  vec3 lightColor2 = vec3(0.6, 0.7, 1.0);             // cool sky

  // Host-side example light setup
  // glUniform3f(lightDir1Loc, 0.5f, 1.0f, 0.8f);
  // glUniform3f(lightColor1Loc, 1.0f, 0.95f, 0.8f);  // Warm sunlight

  // glUniform3f(lightDir2Loc, -0.3f, 0.5f, -0.7f);
  // glUniform3f(lightColor2Loc, 0.6f, 0.7f, 1.0f);   // Cool sky
  //@@@ --------------------------------------------------------

  vec2 uv = (gl_FragCoord.xy / vec2(screenResolution)) * 2.0 - 1.0;

  vec4 nearClip = vec4(uv, -1.0, 1.0);
  vec4 farClip = vec4(uv, 1.0, 1.0);

  vec4 nearWorld = invViewProj * nearClip;
  vec4 farWorld = invViewProj * farClip;

  nearWorld.xyz /= nearWorld.w;
  farWorld.xyz /= farWorld.w;

  vec3 rayOrigin = nearWorld.xyz;
  vec3 rayDir = normalize(farWorld.xyz - nearWorld.xyz);

  float zHalf = normalizedZSpan * 0.5;
  vec3 boxMin = vec3(-0.5, -0.5, -zHalf);
  vec3 boxMax = vec3(0.5, 0.5, zHalf);

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

  int maxSteps = int(length(boxMax - boxMin) / stepSize) * 2;

  for (int i = 0; i < maxSteps; ++i) {
    vec3 boxSize = boxMax - boxMin;
    vec3 normalizedPos = (pos - boxMin) / boxSize;
    normalizedPos.x = 1.0 - normalizedPos.x;  // Flip X if needed

    ivec3 ipos = ivec3(floor(normalizedPos * vec3(resolution)));

    if (all(greaterThanEqual(ipos, ivec3(0))) && all(lessThan(ipos, resolution))) {
      if (isInsideVoxel(ipos.x, ipos.y, ipos.z)) {
        vec3 normal = computeSmoothedNormal(ipos);

        vec3 l1 = normalize(lightDir1);
        vec3 l2 = normalize(lightDir2);

        float diff1 = max(dot(normal, l1), 0.0);
        float diff2 = max(dot(normal, l2), 0.0);

        float ambient = 0.2;

        vec3 finalColor = color * (ambient + diff1 * lightColor1 + diff2 * lightColor2);

        fragColor = vec4(clamp(finalColor, 0.0, 1.0), 1.0);
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


/* // THIS IS OK
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
uniform float normalizedZSpan;

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

    // float normalizedX = gl_FragCoord.x / float(screenResolution.x);
    // fragColor = vec4(normalizedX, 0.0, 0.0, 1.0);
    // return;

    // fragColor = vec4(0.0, 1.0, 0.0, 1.0);
    // return;

    // Calculate normalized device coordinates (NDC) from fragment coordinates, i.e. in the range
[-1, 1] both for x and y vec2 uv = (gl_FragCoord.xy / vec2(screenResolution)) * 2.0 - 1.0;
    // ec2 uv = vec2((gl_FragCoord.x / float(screenResolution.x)) * 2.0 - 1.0, (gl_FragCoord.y /
float(screenResolution.y)) * 2.0 - 1.0);

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
    float zHalf = normalizedZSpan * 0.5;
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
        vec3 boxSize = boxMax - boxMin;
        vec3 normalizedPos = (pos - boxMin) / boxSize;

        normalizedPos.x = 1.0 - normalizedPos.x; //%%% Flip X coordinate to match the voxel grid
orientation ivec3 ipos = ivec3(floor(normalizedPos * vec3(resolution)));


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
} */