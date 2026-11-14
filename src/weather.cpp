#include "weather.hpp"
#include <algorithm>
#include <cmath>
#include <random>

void Weather::init() {
    // dynamic ribbon buffer: per vertex = pos3, side1, bright1, other3 (8 floats)
    glGenVertexArrays(1, &boltVao_);
    glGenBuffers(1, &boltVbo_);
    glBindVertexArray(boltVao_);
    glBindBuffer(GL_ARRAY_BUFFER, boltVbo_);
    glBufferData(GL_ARRAY_BUFFER, 2400 * 4 * 8 * sizeof(float), nullptr, GL_STREAM_DRAW);
    size_t stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(3 * 4));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(4 * 4));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(5 * 4));
    glBindVertexArray(0);

    // rain: kRain streaks, 2 verts each (head, tail), seeds are static
    std::mt19937 rng(2024);
    std::uniform_real_distribution<float> u(0.f, 1.f);
    std::vector<float> rain;
    rain.reserve(kRain * 2 * 4);
    for (int i = 0; i < kRain; ++i) {
        float x = u(rng), y = u(rng), z = u(rng);
        rain.insert(rain.end(), {x, y, z, 0.f, x, y, z, 1.f});
    }
    glGenVertexArrays(1, &rainVao_);
    glGenBuffers(1, &rainVbo_);
    glBindVertexArray(rainVao_);
    glBindBuffer(GL_ARRAY_BUFFER, rainVbo_);
    glBufferData(GL_ARRAY_BUFFER, rain.size() * sizeof(float), rain.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * 4));
    glBindVertexArray(0);
}

void Weather::reset() {
    bolts_.clear();
    thunder_.clear();
    flash_ = 0.f;
}

void Weather::addBolt(glm::vec3 from, glm::vec3 to, uint32_t seed, float strength) {
    Bolt b = generateBolt(from, to, seed, 6);
    b.strength = strength;
    bolts_.push_back(std::move(b));
    lastStrike_ = to;
    lastAge_ = 0.f;
}

void Weather::triggerStrike(glm::vec3 target, uint32_t seed) {
    glm::vec3 from = target + glm::vec3(60.f * std::sin(seed * 1.3f), 650.f, 40.f * std::cos(seed * 0.7f));
    addBolt(from, target, seed, 1.5f);
    thunder_.push_back({0.2f, glm::length(target - lastCenter_) * 0.25f + 40.f});
}

glm::vec3 Weather::flashDir(glm::vec3 camPos) const {
    glm::vec3 d = lastStrike_ - camPos;
    d.y = std::max(d.y, 0.f) * 0.5f + 120.f;
    return glm::normalize(d);
}

void Weather::update(float dt, float t, glm::vec3 center) {
    (void)t;
    lastCenter_ = center;
    // ambient strikes: far away, in a ring around the fleet, mostly on the horizon
    nextAmbient_ -= dt;
    if (nextAmbient_ <= 0.f) {
        uint32_t s = counter_++ * 2654435761u;
        std::mt19937 rng(s);
        std::uniform_real_distribution<float> u(0.f, 1.f);
        float ang = u(rng) * 6.2831853f, dist = 280.f + u(rng) * 520.f;
        glm::vec3 to = center + glm::vec3(std::cos(ang) * dist, 0.f, std::sin(ang) * dist);
        glm::vec3 from = to + glm::vec3((u(rng) - 0.5f) * 200.f, 420.f + u(rng) * 200.f, (u(rng) - 0.5f) * 200.f);
        addBolt(from, to, s, 0.8f + u(rng) * 0.6f);
        thunder_.push_back({0.1f, dist});
        nextAmbient_ = 6.f + u(rng) * 9.f;
    }
    float f = 0.f;
    for (auto& b : bolts_) {
        b.age += dt;
        f = std::max(f, flashEnvelope(b.age) * std::min(1.f, b.strength));
    }
    bolts_.erase(std::remove_if(bolts_.begin(), bolts_.end(), [](const Bolt& b) { return b.age > 1.6f; }), bolts_.end());
    flash_ = f;
    lastAge_ += dt;
}

void Weather::drawBolts(const Shader& sh) {
    if (bolts_.empty()) return;
    std::vector<float> buf;
    std::vector<uint32_t> idx;
    float intensity = 0.f;
    for (const Bolt& b : bolts_) {
        float e = flashEnvelope(b.age);
        // the visible channel flickers on the first pulses only, then fades
        float vis = std::min(1.f, e * 1.6f);
        if (vis <= 0.01f) continue;
        intensity = std::max(intensity, vis * b.strength);
        for (const auto& s : b.segments) {
            if (buf.size() / 8 + 4 > 2400u * 4u) break;
            uint32_t base = (uint32_t)(buf.size() / 8);
            auto push = [&](glm::vec3 p, float side, glm::vec3 other) {
                buf.insert(buf.end(), {p.x, p.y, p.z, side, s.brightness * vis, other.x, other.y, other.z});
            };
            push(s.a, -1.f, s.b); push(s.a, 1.f, s.b); push(s.b, 1.f, s.a); push(s.b, -1.f, s.a);
            idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }
    if (buf.empty()) return;
    sh.use();
    sh.set("uWidth", 0.9f);
    sh.set("uIntensity", 1.0f);
    glBindVertexArray(boltVao_);
    glBindBuffer(GL_ARRAY_BUFFER, boltVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buf.size() * sizeof(float), buf.data());
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STREAM_DRAW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, nullptr);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
    glDeleteBuffers(1, &ebo);
}

void Weather::drawRain(const Shader& sh, glm::vec3 camPos, float intensity) const {
    (void)camPos;
    sh.use();
    sh.set("uWind", glm::vec3(9.f, 0.f, -2.f));
    sh.set("uFall", 13.f);
    sh.set("uBox", glm::vec3(46.f, 30.f, 46.f));
    sh.set("uStreak", 0.9f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glBindVertexArray(rainVao_);
    glDrawArrays(GL_LINES, 0, (GLsizei)(kRain * 2 * intensity));
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
