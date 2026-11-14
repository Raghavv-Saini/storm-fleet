#pragma once
#include "gl_util.hpp"
#include "weather_core.hpp"
#include <vector>

struct ThunderEvent { float delay; float distance; };

class Weather {
public:
    void init();
    // Advance the storm. `center` is where ambient strikes are scattered around.
    void update(float dt, float t, glm::vec3 center);
    void triggerStrike(glm::vec3 target, uint32_t seed);   // bolt that ends at `target`
    float flash() const { return flash_; }
    glm::vec3 flashDir(glm::vec3 camPos) const;            // direction toward the most recent strike
    std::vector<ThunderEvent> takeThunder() { auto v = std::move(thunder_); thunder_.clear(); return v; }
    void reset();

    void drawBolts(const Shader& sh) ;
    void drawRain(const Shader& sh, glm::vec3 camPos, float intensity) const;

private:
    void addBolt(glm::vec3 from, glm::vec3 to, uint32_t seed, float strength);
    std::vector<Bolt> bolts_;
    std::vector<ThunderEvent> thunder_;
    float flash_ = 0.f;
    float nextAmbient_ = 6.f;
    uint32_t counter_ = 1;
    glm::vec3 lastStrike_{0.f, 100.f, 400.f};
    glm::vec3 lastCenter_{0.f};
    float lastAge_ = 99.f;

    GLuint boltVao_ = 0, boltVbo_ = 0;
    GLuint rainVao_ = 0, rainVbo_ = 0;
    static constexpr int kRain = 24000;
};
