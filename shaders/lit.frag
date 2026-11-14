#version 330 core
#include "common.glsl"

in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in vec3 vColor;
in float vMat;
in vec3 vLocal;
in vec4 vShadowPos;

uniform sampler2D uWood;
uniform sampler2D uCanvas;
uniform sampler2D uShadowMap;
uniform float uChar;       // 0..1 burn char on the struck ship
uniform float uWet;        // 0..1 extra wetness
uniform int uShadowPass;
uniform float uSinkWater;  // waterline in world y used to darken submerged parts (unused when < -1000)

layout(location = 0) out vec4 outColor;

float shadowFactor(vec4 sp, vec3 N, vec3 L) {
    if (uCounts.y < 0.5) return 1.0;
    vec3 c = sp.xyz / sp.w * 0.5 + 0.5;
    if (c.z > 1.0 || c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0) return 1.0;
    float bias = max(0.0008 * (1.0 - dot(N, L)), 0.0003);
    float sum = 0.0;
    vec2 texel = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            sum += (c.z - bias > texture(uShadowMap, c.xy + vec2(x, y) * texel).r) ? 0.0 : 1.0;
    return sum / 9.0;
}

void main() {
    if (uShadowPass == 1) { outColor = vec4(1.0); return; }

    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos.xyz - vWorld);
    if (!gl_FrontFacing) N = -N;

    int mat = int(vMat + 0.5);
    vec3 albedo = vColor;
    float rough = 0.7, spec = 0.15, emissive = 0.0;

    if (mat == 0 || mat == 4) {                 // wood / deck planks
        vec4 t = texture(uWood, vUV);
        albedo *= t.rgb * 1.1;
        rough = 0.75 - 0.25 * t.a;
        spec = 0.12;
        if (mat == 4) albedo *= 1.25;
    } else if (mat == 1) {                      // canvas
        albedo *= texture(uCanvas, vUV).rgb;
        rough = 0.95; spec = 0.02;
        // fire-scorched sails
        float soot = smoothstep(0.0, 1.0, uChar) * (0.5 + 0.7 * fbm(vUV * 5.0 + vLocal.xy));
        albedo = mix(albedo, vec3(0.015), clamp(soot, 0.0, 1.0));
    } else if (mat == 3) {                      // emissive (windows, lanterns)
        emissive = 1.0;
    } else if (mat == 5) {                      // rope
        albedo = vColor; rough = 0.9; spec = 0.0;
    }

    // Char: darken wood from the masts and decks outward
    if (mat != 1 && mat != 3) {
        float c = smoothstep(0.0, 1.0, uChar) * (0.35 + 0.65 * fbm(vLocal.xz * 0.7 + vLocal.y * 0.3));
        albedo = mix(albedo, vec3(0.012), clamp(c * 1.1, 0.0, 0.92));
    }

    // Rain-slicked wood is darker and glossier.
    float wet = clamp(uFogColor.a + uWet, 0.0, 1.0);
    albedo *= mix(1.0, 0.38, wet);
    rough = mix(rough, 0.25, wet * 0.8);
    spec = mix(spec, 0.5, wet * 0.6);

    vec3 Lm = normalize(uMoonDir.xyz);
    float sh = shadowFactor(vShadowPos, N, Lm);
    float ndl = max(dot(N, Lm), 0.0);
    vec3 H = normalize(Lm + V);
    float s = pow(max(dot(N, H), 0.0), mix(12.0, 120.0, 1.0 - rough)) * spec;
    vec3 moon = uMoonColor.rgb * (albedo * ndl + s * ndl) * sh;
    if (mat == 1) moon += uMoonColor.rgb * albedo * max(dot(-N, Lm), 0.0) * 0.5 * sh;  // canvas translucency

    // Hemisphere ambient: sky from above, dark sea bounce from below.
    float hemi = N.y * 0.5 + 0.5;
    vec3 amb = mix(vec3(0.004, 0.007, 0.009), uAmbient.rgb, hemi) * albedo;

    vec3 col = moon + amb + pointLights(vWorld, N, V, albedo, rough, spec * 2.0);

    // Warm glow from the lit-up fire onto nearby surfaces is part of pointLights; add emissive
    col += vColor * emissive * 1.0;

    col = applyFog(col, vWorld);
    outColor = vec4(col, 1.0);
}
