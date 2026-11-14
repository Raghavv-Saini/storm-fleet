#pragma once
// Lightning bolt generation and flash envelope (GL-free).
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

struct BoltSegment {
    glm::vec3 a, b;
    float brightness;  // 1 for the main channel, less for branches
};

struct Bolt {
    std::vector<BoltSegment> segments;  // main channel first, in order from `from` to `to`
    int mainCount = 0;
    glm::vec3 from{0.f}, to{0.f};
    float age = 0.f;
    float strength = 1.f;
};

Bolt generateBolt(glm::vec3 from, glm::vec3 to, uint32_t seed, int depth = 6);
// 0..1 flicker envelope of a strike, age in seconds. Zero after ~1.5 s.
float flashEnvelope(float age);
