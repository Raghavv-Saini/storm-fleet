#version 330 core
#include "common.glsl"
in vec2 vCorner;
in vec4 vColor;
uniform int uMode;   // 0 = additive fire/embers, 1 = alpha smoke
layout(location = 0) out vec4 outColor;
void main() {
    float r2 = dot(vCorner, vCorner);
    if (r2 > 1.0) discard;
    float soft = 1.0 - r2;
    if (uMode == 0) {
        float n = 0.65 + 0.35 * vnoise(vCorner * 3.0 + vColor.rg * 17.0);
        float a = soft * soft * n * vColor.a;
        outColor = vec4(vColor.rgb * a, a);
    } else {
        float n = 0.5 + 0.5 * vnoise(vCorner * 2.5 + vColor.r * 31.0);
        float a = soft * n * vColor.a;
        // smoke picks up the flash and firelight
        vec3 col = vColor.rgb * (1.0 + 3.0 * uFlash());
        outColor = vec4(col, a);
    }
}
