#version 330 core
in vec2 vUV;
uniform sampler2D uSrc;   // smaller level
uniform vec2 uTexel;      // 1 / source size
layout(location = 0) out vec4 outColor;
void main() {
    vec3 s = texture(uSrc, vUV).rgb * 4.0;
    s += texture(uSrc, vUV + vec2(uTexel.x, 0)).rgb * 2.0;
    s += texture(uSrc, vUV - vec2(uTexel.x, 0)).rgb * 2.0;
    s += texture(uSrc, vUV + vec2(0, uTexel.y)).rgb * 2.0;
    s += texture(uSrc, vUV - vec2(0, uTexel.y)).rgb * 2.0;
    s += texture(uSrc, vUV + uTexel).rgb;
    s += texture(uSrc, vUV - uTexel).rgb;
    s += texture(uSrc, vUV + vec2(uTexel.x, -uTexel.y)).rgb;
    s += texture(uSrc, vUV + vec2(-uTexel.x, uTexel.y)).rgb;
    outColor = vec4(s / 16.0, 1.0);   // blended additively onto the next level
}
