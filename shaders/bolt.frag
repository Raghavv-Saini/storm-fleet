#version 330 core
in float vSide;
in float vBright;
uniform float uIntensity;
layout(location = 0) out vec4 outColor;
void main() {
    float a = clamp(1.0 - abs(vSide), 0.0, 1.0);
    float core = pow(a, 6.0);
    float glow = pow(a, 1.6);
    vec3 col = vec3(0.55, 0.65, 1.0) * glow * 1.5 + vec3(1.0) * core * 6.0;
    outColor = vec4(col * uIntensity * vBright, 1.0);
}
