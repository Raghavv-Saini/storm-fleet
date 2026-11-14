#pragma once
#include "gl_util.hpp"
#include <cstdint>
#include <vector>

enum class PKind : uint8_t { Fire, Smoke, Ember };

struct Particle {
    glm::vec3 pos, vel;
    float age, life, size, rot, rotVel;
    PKind kind;
};

class Particles {
public:
    static constexpr size_t kMax = 6000;
    void init();
    void spawn(PKind k, glm::vec3 pos, glm::vec3 vel, float life, float size);
    void update(float dt, glm::vec3 wind);
    void clear() { p_.clear(); }
    size_t count() const { return p_.size(); }
    // camPos is used to sort smoke back-to-front.
    void draw(const Shader& sh, glm::vec3 camPos);
    uint32_t rngState = 12345u;
    float rnd() { rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5; return (rngState & 0xFFFFFF) / float(0x1000000); }
private:
    void drawBatch(const Shader& sh, PKind kind, bool smokeMode, glm::vec3 camPos);
    std::vector<Particle> p_;
    std::vector<float> buf_;
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
};
