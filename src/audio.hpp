#pragma once

// Thunder and storm ambience, fully synthesized in code (no audio files).
// Init failure is silent: the program just runs without sound.
namespace audio {
bool init();
void shutdown();
// distance in metres: farther thunder is lower, softer and rumblier. delay in seconds.
void thunder(float distance, float delay);
void setMuted(bool m);
}
