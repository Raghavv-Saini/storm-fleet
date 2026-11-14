#include "ocean_core.hpp"
#include "geometry.hpp"
#include <cmath>

static Wave mk(float angleDeg, float wavelength, float steep) {
    float a = glm::radians(angleDeg);
    float k = 2.0f * 3.14159265f / wavelength;
    Wave w;
    w.dir = glm::vec2(std::cos(a), std::sin(a));
    w.wavelength = wavelength;
    w.amplitude = wavelength / 45.0f;
    // Deep-water dispersion gives the natural period; nudge it (<1%) so a whole number of cycles
    // fits in the 3600 s clock wrap and the sea does not jump when the clock restarts.
    float naturalPeriod = 2.0f * 3.14159265f / std::sqrt(9.81f * k);
    float cycles = std::fmax(1.0f, std::round(3600.0f / naturalPeriod));
    w.speed = wavelength * cycles / 3600.0f;
    w.steepness = steep;
    return w;
}

const std::array<Wave, kWaveCount>& oceanWaves() {
    // Storm swell: wind roughly toward +X, slightly -Z.
    static const std::array<Wave, kWaveCount> waves = {
        mk(-10.f, 60.f, 0.55f), mk(18.f, 37.f, 0.6f), mk(-35.f, 23.f, 0.6f),
        mk(30.f, 14.f, 0.55f),  mk(-22.f, 9.f, 0.5f), mk(5.f, 5.5f, 0.45f)};
    return waves;
}

float wrapTime(float t) { return std::fmod(t, 3600.0f); }

glm::vec3 gerstnerDisplace(glm::vec2 xz, float t, glm::vec3* normalOut) {
    const auto& W = oceanWaves();
    glm::vec3 p(xz.x, 0.0f, xz.y);
    glm::vec3 n(0.0f, 1.0f, 0.0f);
    for (const Wave& w : W) {
        float k = 2.0f * 3.14159265f / w.wavelength;
        float q = w.steepness / (k * w.amplitude * kWaveCount);
        float f = k * (glm::dot(w.dir, xz) - w.speed * t);
        float c = std::cos(f), s = std::sin(f);
        p.x += q * w.amplitude * w.dir.x * c;
        p.z += q * w.amplitude * w.dir.y * c;
        p.y += w.amplitude * s;
        float wa = k * w.amplitude;
        n.x -= w.dir.x * wa * c;
        n.z -= w.dir.y * wa * c;
        n.y -= q * wa * s;
    }
    if (normalOut) *normalOut = glm::normalize(n);
    return p;
}

WaveSample waveSample(glm::vec2 xz, float t) {
    t = wrapTime(t);
    // Find the rest position whose horizontally-displaced point lands on xz.
    glm::vec2 p = xz;
    for (int i = 0; i < 4; ++i) {
        glm::vec3 d = gerstnerDisplace(p, t);
        p -= glm::vec2(d.x, d.z) - xz;
    }
    WaveSample s;
    s.displaced = gerstnerDisplace(p, t, &s.normal);
    s.height = s.displaced.y;
    return s;
}

float waveHeight(glm::vec2 xz, float t) { return waveSample(xz, t).height; }

// ---- MeshData helpers (GL-free, live in core) ----
void MeshData::append(const MeshData& o) {
    uint32_t base = (uint32_t)verts.size();
    verts.insert(verts.end(), o.verts.begin(), o.verts.end());
    for (uint32_t i : o.idx) idx.push_back(base + i);
}

void MeshData::recomputeNormals() {
    for (auto& v : verts) v.normal = glm::vec3(0);
    for (size_t i = 0; i + 2 < idx.size(); i += 3) {
        Vertex &a = verts[idx[i]], &b = verts[idx[i + 1]], &c = verts[idx[i + 2]];
        glm::vec3 n = glm::cross(b.pos - a.pos, c.pos - a.pos);
        a.normal += n; b.normal += n; c.normal += n;
    }
    for (auto& v : verts) {
        float l = glm::length(v.normal);
        v.normal = l > 1e-8f ? v.normal / l : glm::vec3(0, 1, 0);
    }
}
