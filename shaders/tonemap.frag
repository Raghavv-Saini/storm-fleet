#version 330 core
in vec2 vUV;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uExposure;
uniform float uBloomStrength;
uniform float uFlash;
uniform vec2 uRes;
uniform float uTime;
layout(location = 0) out vec4 outColor;

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }

void main() {
    // slight chromatic aberration toward the edges
    vec2 d = vUV - 0.5;
    float ca = dot(d, d) * 0.004;
    vec3 c;
    c.r = texture(uScene, vUV + d * ca).r;
    c.g = texture(uScene, vUV).g;
    c.b = texture(uScene, vUV - d * ca).b;
    c += texture(uBloom, vUV).rgb * uBloomStrength * (1.0 + 0.8 * uFlash);
    c *= uExposure / (1.0 + 0.9 * uFlash);
    // cool, desaturated storm grade: lift blues in shadows
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(l), c, 0.92);
    c += vec3(0.0, 0.003, 0.008) * (1.0 - clamp(l, 0.0, 1.0));
    c = aces(c);
    // vignette
    float v = 1.0 - dot(d, d) * 1.1;
    c *= clamp(v, 0.0, 1.0);
    c = pow(c, vec3(1.0 / 2.2));
    c += (hash(vUV * uRes + uTime) - 0.5) * (1.5 / 255.0);   // dither against banding
    outColor = vec4(c, 1.0);
}
