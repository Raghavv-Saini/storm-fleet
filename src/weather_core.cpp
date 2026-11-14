#include "weather_core.hpp"
#include <cmath>

namespace {
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed * 747796405u + 2891336453u) {}
    float next() {  // 0..1
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (s & 0xFFFFFF) / float(0x1000000);
    }
    float signedNext() { return next() * 2.f - 1.f; }
};

glm::vec3 perpendicular(glm::vec3 d, Rng& r) {
    glm::vec3 t = std::fabs(d.y) < 0.9f ? glm::cross(d, glm::vec3(0, 1, 0)) : glm::cross(d, glm::vec3(1, 0, 0));
    t = glm::normalize(t);
    glm::vec3 b = glm::cross(d, t);
    float ang = r.next() * 6.2831853f;
    return std::cos(ang) * t + std::sin(ang) * b;
}

void subdivide(std::vector<BoltSegment>& path, Rng& r, float offsetScale) {
    std::vector<BoltSegment> out;
    out.reserve(path.size() * 2);
    for (const auto& s : path) {
        glm::vec3 d = s.b - s.a;
        float len = glm::length(d);
        glm::vec3 mid = (s.a + s.b) * 0.5f + perpendicular(d / len, r) * (len * offsetScale * r.next());
        out.push_back({s.a, mid, s.brightness});
        out.push_back({mid, s.b, s.brightness});
    }
    path.swap(out);
}
}  // namespace

Bolt generateBolt(glm::vec3 from, glm::vec3 to, uint32_t seed, int depth) {
    Rng r(seed);
    Bolt bolt;
    bolt.from = from;
    bolt.to = to;
    std::vector<BoltSegment> main{{from, to, 1.f}};
    std::vector<std::vector<BoltSegment>> branches;
    for (int level = 0; level < depth; ++level) {
        subdivide(main, r, 0.25f);
        for (auto& br : branches) subdivide(br, r, 0.25f);
        // spawn branches off random main-channel joints
        if (level >= 1 && level <= 4) {
            size_t n = main.size();
            for (size_t i = 1; i < n; ++i) {
                if (r.next() > 0.35f / (1 << (level - 1))) continue;
                glm::vec3 p = main[i].a;
                glm::vec3 d = glm::normalize(main[i].b - main[i].a);
                float len = glm::length(to - from) / float(1 << level) * 0.5f * (1.f + level * 0.3f);
                glm::vec3 dir = glm::normalize(d + perpendicular(d, r) * (0.6f + 0.6f * r.next()));
                branches.push_back({{p, p + dir * len, 0.45f / (1.f + level * 0.3f)}});
            }
        }
    }
    bolt.segments = main;
    bolt.mainCount = (int)main.size();
    for (auto& br : branches) bolt.segments.insert(bolt.segments.end(), br.begin(), br.end());
    return bolt;
}

float flashEnvelope(float age) {
    if (age < 0.f || age > 1.6f) return 0.f;
    static const float times[3] = {0.f, 0.11f, 0.26f};
    static const float amps[3] = {1.f, 0.65f, 0.85f};
    float f = 0.f;
    for (int i = 0; i < 3; ++i)
        if (age >= times[i]) f += amps[i] * std::exp(-(age - times[i]) * 16.f);
    f += 0.25f * std::exp(-age * 2.5f);
    float fade = 1.f - glm::clamp((age - 1.1f) / 0.5f, 0.f, 1.f);
    return glm::clamp(f * fade, 0.f, 1.f);
}
