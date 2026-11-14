// Shared by every scene shader. Frame-wide uniforms live in one UBO (binding 0).
#define MAX_LIGHTS 16
layout(std140) uniform Frame {
    mat4 uView;
    mat4 uProj;
    mat4 uViewProj;
    mat4 uLightVP;          // moon shadow matrix
    vec4 uCamPos;           // xyz, w = time (wrapped)
    vec4 uMoonDir;          // xyz = direction TO the moon, w = flash 0..1
    vec4 uMoonColor;        // rgb
    vec4 uAmbient;          // rgb sky ambient, a = fog density
    vec4 uFogColor;         // rgb, a = rain wetness 0..1
    vec4 uLightPos[MAX_LIGHTS];   // xyz, w = radius
    vec4 uLightCol[MAX_LIGHTS];   // rgb (HDR), a unused
    vec4 uCounts;           // x = light count, y = shadows on (0/1), z = fire intensity, w = unused
};

float uTime()  { return uCamPos.w; }
float uFlash() { return uMoonDir.w; }

float hash11(float p) { p = fract(p * .1031); p *= p + 33.33; p *= p + p; return fract(p); }
float hash21(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * .1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), u.x), mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), u.x), u.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; ++i) { s += a * vnoise(p); p = p * 2.03 + vec2(17.1, 9.2); a *= 0.5; }
    return s;
}

// Exponential height fog, blended toward the horizon colour.
vec3 applyFog(vec3 col, vec3 worldPos) {
    vec3 d = worldPos - uCamPos.xyz;
    float dist = length(d);
    float heightFalloff = exp(-max(worldPos.y, 0.0) * 0.004);
    float f = 1.0 - exp(-dist * uAmbient.a * heightFalloff);
    vec3 fog = uFogColor.rgb * (1.0 + 1.6 * uFlash());
    return mix(col, fog, clamp(f, 0.0, 1.0));
}

// Point lights: lanterns and fire. Returns radiance contribution for a surface.
vec3 pointLights(vec3 P, vec3 N, vec3 V, vec3 albedo, float rough, float spec) {
    vec3 sum = vec3(0.0);
    int n = int(uCounts.x);
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        if (i >= n) break;
        vec3 L = uLightPos[i].xyz - P;
        float d = length(L);
        float radius = uLightPos[i].w;
        if (d > radius) continue;
        L /= d;
        float att = clamp(1.0 - d / radius, 0.0, 1.0);
        att = att * att / (1.0 + d * d * 0.02);
        float ndl = max(dot(N, L), 0.0);
        vec3 H = normalize(L + V);
        float s = pow(max(dot(N, H), 0.0), mix(8.0, 90.0, 1.0 - rough)) * spec;
        sum += uLightCol[i].rgb * att * (albedo * ndl + s * ndl);
    }
    return sum;
}
