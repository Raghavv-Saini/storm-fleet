#include "textures.hpp"
#include "gl_util.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace {
constexpr float PI = 3.14159265f;

uint32_t hash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
    return x;
}
float h01(int x, int y, int s) { return (hash(x * 73856093u ^ y * 19349663u ^ s * 83492791u) & 0xFFFF) / 65535.f; }

// Tileable value noise with integer period.
float vnoise(float x, float y, int period, int seed) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    auto at = [&](int a, int b) { return h01(((a % period) + period) % period, ((b % period) + period) % period, seed); };
    float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    return (at(xi, yi) * (1 - ux) + at(xi + 1, yi) * ux) * (1 - uy) + (at(xi, yi + 1) * (1 - ux) + at(xi + 1, yi + 1) * ux) * uy;
}
float fbm(float x, float y, int period, int seed, int oct = 5) {
    float s = 0, a = 0.5f, norm = 0;
    for (int i = 0; i < oct; ++i) {
        s += a * vnoise(x, y, period, seed + i);
        norm += a;
        x *= 2; y *= 2; period *= 2; a *= 0.5f;
    }
    return s / norm;
}
unsigned char u8(float v) { return (unsigned char)std::lround(std::fmin(1.f, std::fmax(0.f, v)) * 255.f); }
}  // namespace

GLuint makeWoodTexture(int seed) {
    const int N = 512;
    std::vector<unsigned char> px(N * N * 4);
    const int planks = 4;  // plank rows per texture repeat (along v)
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float u = (float)x / N, v = (float)y / N;
            int row = (int)(v * planks);
            float inRow = v * planks - row;
            // plank butt joints staggered per row
            float jointU = std::fmod(u * 2.f + h01(row, 0, seed) , 1.f);
            float rowTone = 0.78f + 0.3f * h01(row, 1, seed);
            float seg = std::floor(u * 2.f + h01(row, 0, seed));
            rowTone *= 0.92f + 0.16f * h01(row, (int)seg + 7, seed);
            // grain: stretched noise along u
            float g = fbm(u * 4.f, v * 64.f + row * 3.f, 4, seed + 11, 5);
            float rings = 0.5f + 0.5f * std::sin((v * planks * 6.f + g * 7.f) * PI);
            float tone = rowTone * (0.82f + 0.18f * rings) * (0.85f + 0.3f * g);
            // dark seams between planks and at butt joints
            float seam = std::fmin(inRow, 1.f - inRow);
            float seamDark = seam < 0.035f ? 0.35f : 1.f;
            float jointDark = (jointU < 0.006f || jointU > 0.994f) ? 0.45f : 1.f;
            // nail heads
            float nail = 1.f;
            float nu = std::fmod(jointU, 0.5f) * 2.f;
            if ((nu < 0.03f || nu > 0.97f) && std::fabs(inRow - 0.5f) > 0.25f && std::fabs(inRow - 0.5f) < 0.33f) nail = 0.5f;
            float t = tone * seamDark * jointDark * nail;
            int i = (y * N + x) * 4;
            px[i] = u8(t * 1.0f);
            px[i + 1] = u8(t * 0.93f);
            px[i + 2] = u8(t * 0.85f);
            px[i + 3] = u8(0.5f + 0.5f * g * seamDark);
        }
    return makeTexture2D(N, N, px.data(), true, true);
}

GLuint makeCanvasTexture() {
    const int N = 512;
    std::vector<unsigned char> px(N * N * 4);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float u = (float)x / N, v = (float)y / N;
            float weave = 0.9f + 0.1f * (0.5f + 0.5f * std::sin(x * PI * 0.5f)) * (0.5f + 0.5f * std::sin(y * PI * 0.5f));
            // vertical cloth panels (bolts of canvas sewn together)
            float panel = std::fmod(u * 8.f, 1.f);
            float seam = (panel < 0.025f || panel > 0.975f) ? 0.72f : 1.f;
            // horizontal reef band
            float reef = std::fabs(v - 0.18f) < 0.006f ? 0.6f : 1.f;
            float stain = 0.75f + 0.25f * fbm(u * 6.f, v * 6.f, 6, 99, 5);
            float t = weave * seam * reef * stain;
            int i = (y * N + x) * 4;
            px[i] = u8(t);
            px[i + 1] = u8(t * 0.97f);
            px[i + 2] = u8(t * 0.9f);
            px[i + 3] = 255;
        }
    return makeTexture2D(N, N, px.data(), true, true);
}

GLuint makeWaterDetailTexture() {
    const int N = 256;
    std::vector<float> h(N * N);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            float u = (float)x / N, v = (float)y / N;
            float s = 0.f;
            // integer-frequency sinusoids => perfectly tileable chop
            for (int k = 0; k < 24; ++k) {
                float fx = (float)((int)(h01(k, 1, 5) * 12.f) - 6), fy = (float)((int)(h01(k, 2, 5) * 12.f) - 6);
                if (fx == 0 && fy == 0) fx = 1;
                float amp = 1.f / std::sqrt(fx * fx + fy * fy);
                s += amp * std::sin(2 * PI * (fx * u + fy * v) + h01(k, 3, 5) * 6.28f);
            }
            s += 0.6f * fbm(u * 8.f, v * 8.f, 8, 3, 4);
            h[y * N + x] = s;
        }
    std::vector<unsigned char> px(N * N * 4);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) {
            auto H = [&](int a, int b) { return h[((b + N) % N) * N + ((a + N) % N)]; };
            float dx = (H(x + 1, y) - H(x - 1, y)) * 2.f, dy = (H(x, y + 1) - H(x, y - 1)) * 2.f;
            float nx = -dx, ny = -dy, nz = 1.f;
            float l = std::sqrt(nx * nx + ny * ny + nz * nz);
            int i = (y * N + x) * 4;
            px[i] = u8(nx / l * 0.5f + 0.5f);
            px[i + 1] = u8(ny / l * 0.5f + 0.5f);
            px[i + 2] = u8(nz / l * 0.5f + 0.5f);
            px[i + 3] = u8(fbm((float)x / N * 16.f, (float)y / N * 16.f, 16, 77, 4));
        }
    return makeTexture2D(N, N, px.data(), true, true);
}
