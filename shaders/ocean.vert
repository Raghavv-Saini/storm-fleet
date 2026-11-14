#version 330 core
#include "common.glsl"

layout(location = 0) in vec3 aPos;     // rest position (x, 0, z) in camera-snapped grid

uniform vec4 uWaveA[6];  // dir.xy, amplitude, wavelength
uniform vec4 uWaveB[6];  // speed, steepness, unused, unused
uniform vec2 uGridOffset;

out vec3 vWorld;
out vec3 vNormal;
out float vFoam;
out vec2 vRest;

void main() {
    vec2 xz = aPos.xz + uGridOffset;
    vec3 p = vec3(xz.x, 0.0, xz.y);
    vec3 n = vec3(0.0, 1.0, 0.0);
    float jac = 1.0;
    for (int i = 0; i < 6; ++i) {
        vec2 dir = uWaveA[i].xy;
        float amp = uWaveA[i].z, wl = uWaveA[i].w;
        float speed = uWaveB[i].x, steep = uWaveB[i].y;
        float k = 6.2831853 / wl;
        float q = steep / (k * amp * 6.0);
        float f = k * (dot(dir, xz) - speed * uTime());
        float c = cos(f), s = sin(f);
        p.x += q * amp * dir.x * c;
        p.z += q * amp * dir.y * c;
        p.y += amp * s;
        float wa = k * amp;
        n.x -= dir.x * wa * c;
        n.z -= dir.y * wa * c;
        n.y -= q * wa * s;
        jac -= q * wa * s;   // pinches where crests sharpen
    }
    vNormal = normalize(n);
    vFoam = clamp((0.55 - jac) * 2.2, 0.0, 1.0);
    vWorld = p;
    vRest = xz;
    gl_Position = uViewProj * vec4(p, 1.0);
}
