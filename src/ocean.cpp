#include "ocean.hpp"
#include <cmath>
#include <vector>

void Ocean::init() {
    const int N = 384;  // cells per axis
    const float a = 96.f, b = 2400.f;  // x = a*u + b*u^3, u in [-1,1]  -> 0.5 m spacing at the centre, 2.5 km at the edge
    std::vector<float> verts;
    verts.reserve((N + 1) * (N + 1) * 3);
    for (int j = 0; j <= N; ++j) {
        float v = (float)j / (N / 2) - 1.f;
        float z = a * v + b * v * v * v;
        for (int i = 0; i <= N; ++i) {
            float u = (float)i / (N / 2) - 1.f;
            float x = a * u + b * u * u * u;
            verts.insert(verts.end(), {x, 0.f, z});
        }
    }
    std::vector<uint32_t> idx;
    idx.reserve(N * N * 6);
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
            uint32_t p = j * (N + 1) + i;
            idx.insert(idx.end(), {p, p + (uint32_t)N + 1, p + 1, p + 1, p + (uint32_t)N + 1, p + (uint32_t)N + 2});
        }
    count_ = (GLsizei)idx.size();
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(uint32_t), idx.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

void Ocean::draw(const Shader& sh, glm::vec3 cam, GLuint detailTex) const {
    sh.use();
    glm::vec4 A[kWaveCount], B[kWaveCount];
    const auto& W = oceanWaves();
    for (int i = 0; i < kWaveCount; ++i) {
        A[i] = glm::vec4(W[i].dir, W[i].amplitude, W[i].wavelength);
        B[i] = glm::vec4(W[i].speed, W[i].steepness, 0.f, 0.f);
    }
    sh.setArr("uWaveA", A, kWaveCount);
    sh.setArr("uWaveB", B, kWaveCount);
    const float snap = 0.5f;
    sh.set("uGridOffset", glm::vec2(std::floor(cam.x / snap) * snap, std::floor(cam.z / snap) * snap));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, detailTex);
    sh.set("uDetail", 0);
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, count_, GL_UNSIGNED_INT, nullptr);
}
