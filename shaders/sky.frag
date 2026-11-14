#version 330 core
#include "common.glsl"

in vec2 vNdc;
uniform mat4 uInvViewProj;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 w = uInvViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(w.xyz / w.w - uCamPos.xyz);

    float h = clamp(dir.y, 0.0, 1.0);
    vec3 horizon = uFogColor.rgb * 1.5;
    vec3 zenith = vec3(0.012, 0.018, 0.03);
    vec3 col = mix(horizon, zenith, pow(h, 0.45));

    vec3 moon = normalize(uMoonDir.xyz);
    float m = max(dot(dir, moon), 0.0);

    if (dir.y > -0.02) {
        // project onto a cloud plane; clouds scroll with the wind (+X)
        float up = max(dir.y, 0.04);
        vec2 p = dir.xz / up;
        vec2 wind = vec2(uTime() * 0.028, uTime() * 0.007);
        float c1 = fbm(p * 0.22 + wind);
        float c2 = fbm(p * 0.60 + wind * 2.0 + 7.0);
        float cover = smoothstep(0.32, 0.78, c1 * 0.75 + c2 * 0.35);
        float dens = clamp(cover, 0.0, 1.0);
        // clouds lit from behind by the moon, from within by lightning
        vec3 cloudDark = vec3(0.008, 0.011, 0.016);
        vec3 cloudLit = vec3(0.045, 0.055, 0.07) + uMoonColor.rgb * 0.25 * pow(m, 3.0);
        vec3 cloud = mix(cloudDark, cloudLit, smoothstep(0.35, 0.8, c2));
        cloud += vec3(0.5, 0.6, 1.0) * uFlash() * (0.35 + 1.1 * c1) * 1.7;
        // moon disc peeks through thin cloud
        float disc = smoothstep(0.9993, 0.9997, m);
        col += uMoonColor.rgb * 3.0 * disc * (1.0 - dens * 0.92);
        col += uMoonColor.rgb * (0.10 * pow(m, 40.0) + 0.05 * pow(m, 6.0)) * (1.0 - dens * 0.5);
        float fadeHorizon = smoothstep(0.0, 0.18, dir.y);
        col = mix(col, cloud, dens * (0.35 + 0.65 * fadeHorizon));
    }
    col *= 1.0 + 2.6 * uFlash() * (1.0 - h * 0.4);
    outColor = vec4(col, 1.0);
}
