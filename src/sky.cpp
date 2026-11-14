#include "sky.hpp"

void Sky::init() {
    sh_ = Shader::load("sky.vert", "sky.frag");
    glGenVertexArrays(1, &vao_);
}

void Sky::draw(const glm::mat4& invViewProj) const {
    sh_.use();
    sh_.set("uInvViewProj", invViewProj);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDepthMask(GL_TRUE);
}
