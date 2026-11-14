#version 330 core
#include "common.glsl"
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aSide;      // -1 / +1 across the ribbon
layout(location = 2) in float aBright;
layout(location = 3) in vec3 aOther;      // other end of the segment (for ribbon expansion)
uniform float uWidth;
out float vSide;
out float vBright;
void main() {
    vec3 dir = normalize(aOther - aPos);
    vec3 toCam = normalize(uCamPos.xyz - aPos);
    vec3 side = normalize(cross(dir, toCam));
    float dist = length(uCamPos.xyz - aPos);
    float w = uWidth * (0.6 + 0.0025 * dist) * aBright;
    gl_Position = uViewProj * vec4(aPos + side * aSide * w, 1.0);
    vSide = aSide;
    vBright = aBright;
}
