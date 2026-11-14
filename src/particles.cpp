#include "particles.hpp"
#include <algorithm>
#include <cmath>

static const int kFloatsPerVert = 11;  // center3, corner2, color4, size, rot

void Particles::init() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    std::vector<uint32_t> idx;
    idx.reserve(kMax * 6);
    for (uint32_t i = 0; i < kMax; ++i) {
        uint32_t b = i * 4;
        idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * 4, idx.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, kMax * 4 * kFloatsPerVert * sizeof(float), nullptr, GL_STREAM_DRAW);
    size_t stride = kFloatsPerVert * sizeof(float);
    auto attr = [&](GLuint i, GLint n, size_t off) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, n, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)(off * sizeof(float)));
    };
    attr(0, 3, 0); attr(1, 2, 3); attr(2, 4, 5); attr(3, 1, 9); attr(4, 1, 10);
    glBindVertexArray(0);
    buf_.reserve(kMax * 4 * kFloatsPerVert);
}

void Particles::spawn(PKind k, glm::vec3 pos, glm::vec3 vel, float life, float size) {
    if (p_.size() >= kMax) return;
    p_.push_back({pos, vel, 0.f, life, size, rnd() * 6.28f, (rnd() - 0.5f) * 1.5f, k});
}

void Particles::update(float dt, glm::vec3 wind) {
    for (auto& q : p_) {
        q.age += dt;
        glm::vec3 target = wind;
        switch (q.kind) {
            case PKind::Fire:  q.vel += glm::vec3(0, 2.5f, 0) * dt; q.vel = glm::mix(q.vel, target * 0.3f + glm::vec3(0, q.vel.y, 0), dt * 0.8f); break;
            case PKind::Smoke: q.vel += glm::vec3(0, 1.0f, 0) * dt; q.vel = glm::mix(q.vel, target + glm::vec3(0, q.vel.y, 0), dt * 0.5f); break;
            case PKind::Ember: q.vel += glm::vec3(0, -1.5f, 0) * dt; q.vel += target * dt * 0.6f; break;
        }
        q.pos += q.vel * dt;
        q.rot += q.rotVel * dt;
    }
    p_.erase(std::remove_if(p_.begin(), p_.end(), [](const Particle& q) { return q.age >= q.life; }), p_.end());
}

static glm::vec4 colorFor(const Particle& q) {
    float f = q.age / q.life;
    switch (q.kind) {
        case PKind::Fire: {
            // white-yellow core -> orange -> deep red, fading out
            glm::vec3 c = glm::mix(glm::vec3(4.6f, 2.3f, 0.7f), glm::vec3(1.9f, 0.42f, 0.06f), glm::smoothstep(0.f, 0.5f, f));
            c = glm::mix(c, glm::vec3(0.35f, 0.04f, 0.01f), glm::smoothstep(0.5f, 1.f, f));
            return glm::vec4(c, (1.f - f) * (1.f - f) * 1.1f);
        }
        case PKind::Ember: {
            float flick = 0.6f + 0.4f * std::sin(q.age * 40.f + q.rot * 7.f);
            return glm::vec4(glm::vec3(7.f, 2.6f, 0.5f) * flick, 1.f - f);
        }
        default: {
            float g = 0.04f + 0.025f * std::fmod(q.rot, 1.f);
            glm::vec3 grey(g, g * 1.02f, g * 1.06f);
            // freshly emitted smoke is underlit by the flames, then cools to grey as it rises
            glm::vec3 c = glm::mix(glm::vec3(0.55f, 0.2f, 0.05f), grey, glm::smoothstep(0.f, 0.28f, f));
            return glm::vec4(c, 0.42f * std::sin(glm::clamp(f, 0.f, 1.f) * 3.14159f) * (1.f - 0.3f * f));
        }
    }
}

void Particles::drawBatch(const Shader& sh, PKind kind, bool smokeMode, glm::vec3 camPos) {
    std::vector<const Particle*> sel;
    for (const auto& q : p_) if (q.kind == kind || (kind == PKind::Fire && q.kind == PKind::Ember)) sel.push_back(&q);
    if (sel.empty()) return;
    if (smokeMode)
        std::sort(sel.begin(), sel.end(), [&](const Particle* a, const Particle* b) {
            return glm::dot(a->pos - camPos, a->pos - camPos) > glm::dot(b->pos - camPos, b->pos - camPos);
        });
    buf_.clear();
    static const glm::vec2 corners[4] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (const Particle* q : sel) {
        glm::vec4 c = colorFor(*q);
        float f = q->age / q->life;
        float size = q->size * (q->kind == PKind::Smoke ? (0.6f + 2.8f * f) : q->kind == PKind::Fire ? (1.f - 0.55f * f) : 1.f);
        for (int k = 0; k < 4; ++k)
            buf_.insert(buf_.end(), {q->pos.x, q->pos.y, q->pos.z, corners[k].x, corners[k].y, c.r, c.g, c.b, c.a, size, q->rot});
    }
    sh.set("uMode", smokeMode ? 1 : 0);
    sh.set("uStretch", smokeMode ? 1.0f : 1.7f);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, buf_.size() * sizeof(float), buf_.data());
    glDrawElements(GL_TRIANGLES, (GLsizei)(sel.size() * 6), GL_UNSIGNED_INT, nullptr);
}

void Particles::draw(const Shader& sh, glm::vec3 camPos) {
    sh.use();
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // smoke: shader outputs rgb un-premultiplied -> handled below
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    drawBatch(sh, PKind::Smoke, true, camPos);
    glBlendFunc(GL_ONE, GL_ONE);                   // fire + embers: additive
    drawBatch(sh, PKind::Fire, false, camPos);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}
