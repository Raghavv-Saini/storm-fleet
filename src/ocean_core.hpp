#pragma once
// Gerstner wave model. The same formula runs in shaders/ocean.vert and
// shaders/common.glsl; the CPU side lets ships float on the rendered water.
#include <glm/glm.hpp>
#include <array>

struct Wave {
    glm::vec2 dir;      // normalized horizontal direction
    float amplitude;    // metres
    float wavelength;   // metres
    float speed;        // phase speed m/s
    float steepness;    // 0..1 (Gerstner Q factor before normalisation)
};

constexpr int kWaveCount = 6;
const std::array<Wave, kWaveCount>& oceanWaves();

struct WaveSample {
    glm::vec3 displaced;  // displaced position of the surface point that started at xz
    glm::vec3 normal;     // surface normal there
    float height;         // water height directly above/below world xz
};

// Displacement of a surface point whose rest position is xz.
glm::vec3 gerstnerDisplace(glm::vec2 xz, float t, glm::vec3* normalOut = nullptr);
WaveSample waveSample(glm::vec2 xz, float t);
float waveHeight(glm::vec2 xz, float t);
// Time sent to shaders and used for wave sampling, kept small for float precision.
float wrapTime(float t);
