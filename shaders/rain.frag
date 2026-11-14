#version 330 core
#include "common.glsl"
in float vA;
layout(location = 0) out vec4 outColor;
void main() {
    vec3 col = vec3(0.35, 0.42, 0.55) * (0.5 + 2.5 * uFlash() * 3.0) + uFogColor.rgb * 0.6;
    outColor = vec4(col * vA, vA * 0.4);
}
