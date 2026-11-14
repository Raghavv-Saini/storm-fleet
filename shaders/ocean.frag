#version 330 core
#include "common.glsl"

in vec3 vWorld;
in vec3 vNormal;
in float vFoam;
in vec2 vRest;

uniform sampler2D uDetail;   // tileable normal map + noise
layout(location = 0) out vec4 outColor;

// Must match shaders/sky.frag so reflections line up with the visible sky.
vec3 skyColor(vec3 dir) {
    float h = clamp(dir.y, 0.0, 1.0);
    vec3 horizon = uFogColor.rgb * 1.5;
    vec3 zenith = vec3(0.012, 0.018, 0.03);
    vec3 c = mix(horizon, zenith, pow(h, 0.45));
    // moon glow through the cloud deck
    float m = max(dot(dir, normalize(uMoonDir.xyz)), 0.0);
    c += uMoonColor.rgb * (0.12 * pow(m, 40.0) + 0.04 * pow(m, 6.0));
    c *= 1.0 + 2.6 * uFlash();
    return c;
}

void main() {
    // Two scrolling detail normal layers for fine chop.
    vec2 uv1 = vRest * 0.07 + vec2(uTime() * 0.03, uTime() * 0.01);
    vec2 uv2 = vRest * 0.23 - vec2(uTime() * 0.05, -uTime() * 0.02);
    vec3 d1 = texture(uDetail, uv1).xyz * 2.0 - 1.0;
    vec3 d2 = texture(uDetail, uv2).xyz * 2.0 - 1.0;
    vec3 N = normalize(vNormal + vec3(d1.x + d2.x, 0.0, d1.y + d2.y) * 0.18);

    vec3 V = normalize(uCamPos.xyz - vWorld);
    float ndv = clamp(dot(N, V), 0.0, 1.0);
    float fres = 0.02 + 0.98 * pow(1.0 - ndv, 5.0);

    vec3 R = reflect(-V, N);
    R.y = abs(R.y) * 0.8 + 0.02;
    vec3 refl = skyColor(normalize(R));

    // Deep water with subsurface scattering lighting up the crests.
    vec3 deep = vec3(0.003, 0.009, 0.014);
    vec3 shallow = vec3(0.010, 0.030, 0.038);
    float crest = clamp(vWorld.y * 0.16 + 0.35, 0.0, 1.0);
    vec3 col = mix(deep, shallow, crest);
    vec3 Lm = normalize(uMoonDir.xyz);
    float sss = pow(clamp(dot(-V, Lm) * 0.5 + 0.5, 0.0, 1.0), 4.0) * crest;
    col += vec3(0.02, 0.08, 0.07) * sss * (1.0 + 2.0 * uFlash());
    col += (uMoonColor.rgb * 0.03 + uAmbient.rgb) * 0.25 * crest;

    col = mix(col, refl, fres);

    // Specular streaks from the moon / lightning.
    vec3 H = normalize(Lm + V);
    float spec = pow(max(dot(N, H), 0.0), 220.0);
    col += uMoonColor.rgb * spec * 1.1 * (1.0 + 2.0 * uFlash());

    // Firelight and lantern light bouncing off the sea.
    col += pointLights(vWorld, N, V, vec3(0.02, 0.04, 0.04), 0.3, 0.5) * 0.35;

    // Foam: sharp crests plus noise breakup.
    float fn = texture(uDetail, vRest * 0.11 + vec2(uTime() * 0.02, 0.0)).a;
    float foam = smoothstep(0.35, 0.9, vFoam * (0.5 + fn)) ;
    vec3 foamCol = vec3(0.55, 0.6, 0.65) * (0.35 + 0.65 * max(dot(N, Lm), 0.0) + 0.6 * uFlash() * 1.6)
                 + uAmbient.rgb * 0.6;
    foamCol += pointLights(vWorld, vec3(0, 1, 0), V, vec3(0.6), 0.8, 0.0) * 0.5;
    col = mix(col, foamCol, foam * 0.85);

    col = applyFog(col, vWorld);
    outColor = vec4(col, 1.0);
}
