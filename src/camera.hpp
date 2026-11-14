#pragma once
#include "ship_core.hpp"
#include <glm/glm.hpp>

struct InputState {
    bool w = false, a = false, s = false, d = false, up = false, down = false, fast = false;
    float mouseDx = 0.f, mouseDy = 0.f;
};

// yaw 0 looks toward +Z; pitch positive looks up.
glm::vec3 lookDir(float yaw, float pitch);

struct FlyCamera {
    glm::vec3 pos{0.f, 25.f, -70.f};
    float yaw = 0.f, pitch = -0.15f;
    glm::mat4 view() const;
    void update(const InputState& in, float dt);
};

glm::vec2 clampToDeck(glm::vec2 local, const DeckOutline& o, float margin = 0.4f);

struct DeckCamera {
    glm::vec2 local{0.f, -4.f};  // (x, z) on the deck in ship-local space
    float yaw = 0.f, pitch = 0.05f;
    float bob = 0.f;             // walking head-bob phase
    glm::vec3 eyeLocal(const DeckOutline& o) const;
    glm::mat4 view(const glm::mat4& shipModel, const DeckOutline& o) const;
    void update(const InputState& in, float dt, const DeckOutline& o);
};
