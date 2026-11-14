#version 330 core
#include "common.glsl"
layout(location = 0) in vec3 aCenter;
layout(location = 1) in vec2 aCorner;     // -1..1
layout(location = 2) in vec4 aColor;      // premultiplied-ish rgba (HDR rgb)
layout(location = 3) in float aSize;
layout(location = 4) in float aRot;
uniform float uStretch;
out vec2 vCorner;
out vec4 vColor;
void main() {
    vec3 right = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 up    = vec3(uView[0][1], uView[1][1], uView[2][1]);
    float rr = uStretch > 1.0 ? 0.0 : aRot;
    float c = cos(rr), s = sin(rr);
    vec2 q = vec2(c * aCorner.x - s * aCorner.y, s * aCorner.x + c * aCorner.y);
    q.x *= (uStretch > 1.0 ? 0.62 : 1.0);
    q.y *= uStretch;
    if (uStretch > 1.0) q.y += 0.55;   // flames rise from the emitter point
    vec3 p = aCenter + (right * q.x + up * q.y) * aSize;
    gl_Position = uViewProj * vec4(p, 1.0);
    vCorner = aCorner;
    vColor = aColor;
}
