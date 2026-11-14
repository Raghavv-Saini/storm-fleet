#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

glm::vec3 lookDir(float yaw, float pitch) {
    return glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
}

static void applyMouse(float& yaw, float& pitch, const InputState& in) {
    const float sens = 0.0025f;
    yaw -= in.mouseDx * sens;
    pitch -= in.mouseDy * sens;
    pitch = glm::clamp(pitch, -1.5f, 1.5f);
}

glm::mat4 FlyCamera::view() const {
    return glm::lookAt(pos, pos + lookDir(yaw, pitch), glm::vec3(0, 1, 0));
}

void FlyCamera::update(const InputState& in, float dt) {
    applyMouse(yaw, pitch, in);
    glm::vec3 f = lookDir(yaw, pitch);
    glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0, 1, 0)));
    glm::vec3 v(0.f);
    if (in.w) v += f;
    if (in.s) v -= f;
    if (in.d) v += r;
    if (in.a) v -= r;
    if (in.up) v.y += 1.f;
    if (in.down) v.y -= 1.f;
    if (glm::length(v) > 0.f) pos += glm::normalize(v) * (in.fast ? 60.f : 15.f) * dt;
    pos.y = glm::clamp(pos.y, 2.0f, 600.f);
}

glm::vec2 clampToDeck(glm::vec2 local, const DeckOutline& o, float margin) {
    glm::vec2 r = local;
    r.y = glm::clamp(r.y, o.zMin + margin, o.zMax - margin);
    float hb = o.halfBeam(r.y) - margin;
    r.x = hb <= 0.f ? 0.f : glm::clamp(r.x, -hb, hb);
    return r;
}

glm::vec3 DeckCamera::eyeLocal(const DeckOutline& o) const {
    float bobY = 0.04f * std::sin(bob * 2.f);
    return glm::vec3(local.x, o.deckHeight(local.y) + 1.7f + bobY, local.y);
}

glm::mat4 DeckCamera::view(const glm::mat4& shipModel, const DeckOutline& o) const {
    glm::vec3 eye = eyeLocal(o);
    glm::mat4 localView = glm::lookAt(eye, eye + lookDir(yaw, pitch), glm::vec3(0, 1, 0));
    return localView * glm::inverse(shipModel);
}

void DeckCamera::update(const InputState& in, float dt, const DeckOutline& o) {
    applyMouse(yaw, pitch, in);
    glm::vec2 f(std::sin(yaw), std::cos(yaw));
    glm::vec2 r(-f.y, f.x);  // = cross(forward, up) in (x, z)
    glm::vec2 v(0.f);
    if (in.w) v += f;
    if (in.s) v -= f;
    if (in.d) v += r;
    if (in.a) v -= r;
    if (glm::length(v) > 0.f) {
        float speed = in.fast ? 5.5f : 2.6f;
        local += glm::normalize(v) * speed * dt;
        bob += dt * speed * 2.2f;
    }
    local = clampToDeck(local, o);
}
