#include "audio.hpp"

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <random>
#include <vector>

namespace audio {
namespace {
constexpr int kRate = 44100;
ma_device g_dev;
bool g_ok = false;
std::atomic<bool> g_muted{false};
std::mutex g_mx;

struct Voice { const std::vector<float>* buf; size_t pos; float gain; long delay; };
std::vector<Voice> g_voices;
std::vector<float> g_clips[3];   // short, medium, long rumbles
std::vector<float> g_rain;       // looping rain-bed noise
size_t g_rainPos = 0;

// Thunder = crack (broadband burst) + low-passed brown-noise rumble with a slow decay and rolling swells.
std::vector<float> synth(float seconds, float crackiness, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    size_t n = (size_t)(seconds * kRate);
    std::vector<float> out(n);
    float brown = 0.f, lp1 = 0.f, lp2 = 0.f;
    float swellPhase = u(rng) * 3.f;
    for (size_t i = 0; i < n; ++i) {
        float t = (float)i / kRate;
        float w = u(rng);
        brown = (brown + 0.02f * w) / 1.02f;           // brown noise
        float cutoff = 0.02f + 0.10f * std::exp(-t * 0.9f);
        lp1 += cutoff * (brown * 14.f - lp1);
        lp2 += cutoff * (lp1 - lp2);
        float env = std::exp(-t * 0.55f) * (1.f - std::exp(-t * 18.f));
        float swell = 0.65f + 0.35f * std::sin(t * 2.3f + swellPhase) * std::sin(t * 0.9f + swellPhase * 2.f);
        float rumble = lp2 * env * swell;
        float crack = w * std::exp(-t * 14.f) * crackiness;  // short sharp onset
        out[i] = (rumble * 1.6f + crack) * 0.9f;
    }
    // normalise
    float peak = 1e-6f;
    for (float v : out) peak = std::fmax(peak, std::fabs(v));
    for (float& v : out) v = std::tanh(v / peak * 1.4f) * 0.8f;
    return out;
}

void callback(ma_device*, void* pOut, const void*, ma_uint32 frames) {
    float* out = (float*)pOut;
    std::lock_guard<std::mutex> lk(g_mx);
    for (ma_uint32 i = 0; i < frames; ++i) {
        float l = 0.f, r = 0.f;
        if (!g_muted && !g_rain.empty()) {
            float s = g_rain[g_rainPos++] * 0.05f;
            if (g_rainPos >= g_rain.size()) g_rainPos = 0;
            l += s; r += g_rain[(g_rainPos + 977) % g_rain.size()] * 0.05f;
        }
        for (auto& v : g_voices) {
            if (v.delay > 0) { --v.delay; continue; }
            if (v.pos >= v.buf->size()) continue;
            float s = (*v.buf)[v.pos++] * v.gain;
            l += s; r += s;
        }
        if (g_muted) { l = r = 0.f; }
        out[i * 2] = l;
        out[i * 2 + 1] = r;
    }
    g_voices.erase(std::remove_if(g_voices.begin(), g_voices.end(), [](const Voice& v) { return v.delay <= 0 && v.pos >= v.buf->size(); }),
                   g_voices.end());
}
}  // namespace

bool init() {
    g_clips[0] = synth(3.5f, 1.0f, 11);
    g_clips[1] = synth(6.0f, 0.6f, 22);
    g_clips[2] = synth(9.0f, 0.25f, 33);
    // rain bed: filtered white noise, loops
    {
        std::mt19937 rng(5);
        std::uniform_real_distribution<float> u(-1.f, 1.f);
        g_rain.resize(kRate * 4);
        float lp = 0.f, hp = 0.f;
        for (auto& s : g_rain) { float w = u(rng); lp += 0.25f * (w - lp); hp += 0.02f * (lp - hp); s = lp - hp; }
        // crossfade the loop seam
        size_t f = 2000;
        for (size_t i = 0; i < f; ++i) {
            float a = (float)i / f;
            g_rain[i] = g_rain[i] * a + g_rain[g_rain.size() - f + i] * (1.f - a);
        }
        g_rain.resize(g_rain.size() - f);
    }
    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 2;
    cfg.sampleRate = kRate;
    cfg.dataCallback = callback;
    if (ma_device_init(nullptr, &cfg, &g_dev) != MA_SUCCESS) return false;
    if (ma_device_start(&g_dev) != MA_SUCCESS) { ma_device_uninit(&g_dev); return false; }
    g_ok = true;
    return true;
}

void shutdown() {
    if (g_ok) { ma_device_uninit(&g_dev); g_ok = false; }
}

void setMuted(bool m) { g_muted = m; }

void thunder(float distance, float delay) {
    if (!g_ok) return;
    int clip = distance < 250.f ? 0 : distance < 550.f ? 1 : 2;
    float gain = 1.2f / (1.f + distance * 0.004f);
    std::lock_guard<std::mutex> lk(g_mx);
    if (g_voices.size() > 8) return;
    g_voices.push_back({&g_clips[clip], 0, gain, (long)(delay * kRate)});
}
}  // namespace audio
