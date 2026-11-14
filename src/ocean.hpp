#pragma once
#include "gl_util.hpp"
#include "ocean_core.hpp"

// Camera-following ocean grid; vertex spacing grows with distance (0.5 m near, coarse to the horizon).
class Ocean {
public:
    void init();
    void draw(const Shader& sh, glm::vec3 camPos, GLuint detailTex) const;
private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei count_ = 0;
};
