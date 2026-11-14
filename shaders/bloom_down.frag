#version 330 core
// First level applies a soft bright-pass; later levels are plain 13-tap-style downsamples.
in vec2 vUV;
uniform sampler2D uSrc;
uniform vec2 uTexel;     // 1 / source size
uniform int uFirst;
layout(location = 0) out vec4 outColor;
vec3 bright(vec3 c) {
    float l = max(c.r, max(c.g, c.b));
    float w = clamp((l - 1.0) / 1.5, 0.0, 1.0);
    return c * w;
}
void main() {
    vec3 s = vec3(0.0);
    vec2 o[4] = vec2[](vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
    for (int i = 0; i < 4; ++i) {
        vec3 c = texture(uSrc, vUV + o[i] * uTexel).rgb;
        s += (uFirst == 1) ? bright(min(c, vec3(30.0))) : c;
    }
    outColor = vec4(s * 0.25, 1.0);
}
