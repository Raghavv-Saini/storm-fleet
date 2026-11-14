#version 330 core
#include "common.glsl"

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec3 aColor;
layout(location = 4) in float aMat;

uniform mat4 uModel;
uniform float uBillow;      // 0 = rigid, 1 = sails billow/flutter
uniform float uShipSeed;    // phase offset so ships don't flutter in unison
uniform mat4 uShadowVP;
uniform int uShadowPass;

out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out vec3 vColor;
out float vMat;
out vec3 vLocal;
out vec4 vShadowPos;

void main() {
    vec3 p = aPos;
    vec3 n = aNormal;
    if (uBillow > 0.5 && aMat > 0.5 && aMat < 1.5) {
        // canvas: billow with gusts and a fast flutter near free edges
        float t = uTime() + uShipSeed;
        float gust = 0.5 + 0.5 * sin(t * 0.9 + p.y * 0.15) * sin(t * 0.37 + p.z * 0.05);
        float belly = sin(clamp(aUV.x, 0.0, 1.0) * 3.14159) * sin(clamp(aUV.y, 0.0, 1.0) * 3.14159);
        p.z += belly * (0.5 + 1.4 * gust);
        p.x += 0.15 * sin(t * 3.1 + p.y * 1.7 + p.z) * (1.0 - belly * 0.5) * (0.3 + gust);
        p.y += 0.06 * sin(t * 2.3 + p.x * 2.0);
        n = normalize(n + vec3(0.0, 0.0, 0.25 * belly));
    }
    vLocal = p;
    vec4 w = uModel * vec4(p, 1.0);
    vWorld = w.xyz;
    vNormal = mat3(uModel) * n;     // uniform scale only
    vUV = aUV;
    vColor = aColor;
    vMat = aMat;
    vShadowPos = uShadowVP * w;
    gl_Position = (uShadowPass == 1 ? uShadowVP : uViewProj) * w;
}
