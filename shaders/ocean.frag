#version 330 core
#include "common.glsl"

in vec3 vWorld;
in vec3 vNormal;
in float vFoam;
in vec2 vRest;

uniform sampler2D uDetail;   // tileable normal map + noise
uniform vec4 uShipA[6];      // x, z, heading, length
uniform vec4 uShipB[6];      // beam, sink churn 0..1, speed factor, unused
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

// Foam the ships stir up: a ring where the hull meets the sea, a widening V-wake astern with
// bright edges, and a churning whirlpool where a ship is going down.
float shipFoam(vec3 w, float n) {
    float f = 0.0;
    for (int i = 0; i < 6; ++i) {
        vec2 d = w.xz - uShipA[i].xy;
        float L = uShipA[i].w, B = uShipB[i].x;
        if (dot(d, d) > (L * 4.0) * (L * 4.0) && uShipB[i].y < 0.01) continue;
        float h = uShipA[i].z;
        vec2 fw = vec2(sin(h), cos(h));
        vec2 rt = vec2(fw.y, -fw.x);
        float lz = dot(d, fw), lx = dot(d, rt);
        float e = length(vec2(lx / (B * 0.5 + 1.0), lz / (L * 0.5 + 1.6)));
        float ring = smoothstep(0.72, 1.0, e) * (1.0 - smoothstep(1.0, 1.4, e));
        float bowSurge = smoothstep(0.1, 0.5, lz / L) * ring * 0.8;   // heavier foam piled up at the bow
        float back = -lz - L * 0.42;
        float inWake = step(0.0, back);
        float fade = exp(-max(back, 0.0) * 0.022);
        float wakeW = B * 0.55 + back * 0.2;
        float centre = inWake * smoothstep(wakeW, wakeW * 0.15, abs(lx)) * fade * 0.7;
        float edge = inWake * exp(-pow((abs(lx) - (B * 0.5 + back * 0.3)) * 0.45, 2.0)) * fade * 0.8;
        f += (ring * 0.7 + bowSurge * 0.8 + (centre + edge) * uShipB[i].z);
        // sinking whirlpool: expanding disc of churned foam with a swirling breakup
        float sink = uShipB[i].y;
        if (sink > 0.0) {
            float r = length(d) / (L * (0.35 + 0.5 * sink));
            float swirl = vnoise(vec2(atan(d.y, d.x) * 3.0, r * 6.0 - uTime() * 1.5));
            f += sink * smoothstep(1.2, 0.2, r) * (0.55 + 0.9 * swirl);
        }
    }
    float n2 = vnoise(w.xz * 1.7 + vec2(uTime() * 0.4, -uTime() * 0.25));   // fine breakup so foam is patchy, not painted
    return clamp(f * (0.15 + 1.4 * n * (0.35 + n2)), 0.0, 1.0);
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
    float foam = smoothstep(0.35, 0.9, vFoam * (0.5 + fn));
    foam = max(foam, smoothstep(0.25, 0.8, shipFoam(vWorld, fn)));
    vec3 foamCol = vec3(0.42, 0.47, 0.52) * (0.35 + 0.65 * max(dot(N, Lm), 0.0) + 0.6 * uFlash() * 1.6)
                 + uAmbient.rgb * 0.6;
    foamCol += pointLights(vWorld, vec3(0, 1, 0), V, vec3(0.6), 0.8, 0.0) * 0.22;
    col = mix(col, foamCol, foam * 0.85);

    col = applyFog(col, vWorld);
    outColor = vec4(col, 1.0);
}
