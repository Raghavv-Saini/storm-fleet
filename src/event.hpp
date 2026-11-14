#pragma once
// Lightning-strike event timeline for the struck ship. A pure function of time,
// so both scenes always see the same phase.

enum class Phase { Calm, Strike, Burning, Sinking, Sunk };

struct StrikeEvent {
    float t0 = 20.f;  // time of the strike (seconds since start)

    static constexpr float kStrikeLen = 0.3f;
    static constexpr float kSinkStart = 40.f;
    static constexpr float kSinkEnd = 90.f;

    Phase phase(float t) const;
    float sinkProgress(float t) const;   // 0..1
    float fireIntensity(float t) const;  // 0..1
    float burnTime(float t) const;       // seconds since strike, 0 before
    void retrigger(float now, float delay = 3.f) { t0 = now + delay; }
};
