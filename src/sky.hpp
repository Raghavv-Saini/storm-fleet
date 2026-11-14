#pragma once
#include "gl_util.hpp"

// Fullscreen procedural sky: gradient, scrolling fbm clouds, moon glow, lightning-lit clouds.
class Sky {
public:
    void init();
    void draw(const glm::mat4& invViewProj) const;
private:
    Shader sh_;
    GLuint vao_ = 0;
};
