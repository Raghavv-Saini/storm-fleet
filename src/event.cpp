#include "event.hpp"
#include <algorithm>

static float smooth01(float x) {
    x = std::min(1.f, std::max(0.f, x));
    return x * x * (3.f - 2.f * x);
}

Phase StrikeEvent::phase(float t) const {
    float d = t - t0;
    if (d < 0.f) return Phase::Calm;
    if (d < kStrikeLen) return Phase::Strike;
    if (d < kSinkStart) return Phase::Burning;
    if (d < kSinkEnd) return Phase::Sinking;
    return Phase::Sunk;
}

float StrikeEvent::burnTime(float t) const { return std::max(0.f, t - t0); }

float StrikeEvent::sinkProgress(float t) const {
    return smooth01((t - t0 - kSinkStart) / (kSinkEnd - kSinkStart));
}

float StrikeEvent::fireIntensity(float t) const {
    float d = t - t0;
    if (d < 0.f) return 0.f;
    float rampUp = smooth01(d / 8.f);
    float drown = 1.f - smooth01((sinkProgress(t) - 0.35f) / 0.4f);
    return std::max(0.f, rampUp * drown);
}
