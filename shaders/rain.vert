#version 330 core
#include "common.glsl"
layout(location = 0) in vec3 aSeed;       // random 0..1 xyz
layout(location = 1) in float aEnd;       // 0 = head, 1 = tail
uniform vec3 uWind;                       // horizontal drift m/s
uniform float uFall;                      // fall speed m/s
uniform vec3 uBox;                        // box size
uniform float uStreak;                    // streak length m
out float vA;
void main() {
    vec3 vel = vec3(uWind.x, -uFall, uWind.z);
    vec3 p = aSeed * uBox + vel * uTime();
    vec3 origin = uCamPos.xyz - uBox * 0.5;
    p = origin + mod(p - origin, uBox);
    vec3 dir = normalize(vel);
    p -= dir * uStreak * aEnd;
    gl_Position = uViewProj * vec4(p, 1.0);
    float dist = length(p - uCamPos.xyz);
    vA = (1.0 - aEnd * 0.9) * clamp(1.4 - dist / uBox.x * 1.6, 0.0, 1.0) * (0.5 + 0.5 * fract(aSeed.x * 91.7));
}
