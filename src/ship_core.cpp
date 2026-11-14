#include "ship_core.hpp"
#include "ocean_core.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace {
constexpr float PI = 3.14159265f;

float sm(float a, float b, float x) {  // smoothstep that also works with a > b
    float t = glm::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Vertex V(glm::vec3 p, glm::vec2 uv, glm::vec3 c, int mat, glm::vec3 n = glm::vec3(0, 1, 0)) {
    return Vertex{p, n, uv, c, (float)mat};
}

// Add a quad and flip its winding so the face normal agrees with `hint`.
void orientQuad(MeshData& m, uint32_t a, uint32_t b, uint32_t c, uint32_t d, glm::vec3 hint) {
    glm::vec3 n = glm::cross(m.verts[b].pos - m.verts[a].pos, m.verts[c].pos - m.verts[a].pos);
    if (glm::dot(n, hint) >= 0) m.quad(a, b, c, d);
    else m.quad(a, d, c, b);
}

void flatQuad(MeshData& m, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 col, int mat,
              glm::vec3 hint, glm::vec2 uvScale = glm::vec2(1)) {
    glm::vec3 cr = glm::cross(b - a, c - a);
    if (glm::length(cr) < 1e-6f) cr = glm::cross(c - a, d - a);
    if (glm::length(cr) < 1e-6f) return;  // fully degenerate (e.g. bulwark where the hull closes at the bow)
    glm::vec3 n = glm::normalize(cr);
    if (glm::dot(n, hint) < 0) { std::swap(b, d); n = -n; }
    uint32_t i0 = m.add(V(a, glm::vec2(0, 0) * uvScale, col, mat, n));
    uint32_t i1 = m.add(V(b, glm::vec2(1, 0) * uvScale, col, mat, n));
    uint32_t i2 = m.add(V(c, glm::vec2(1, 1) * uvScale, col, mat, n));
    uint32_t i3 = m.add(V(d, glm::vec2(0, 1) * uvScale, col, mat, n));
    m.quad(i0, i1, i2, i3);
}

void box(MeshData& m, glm::vec3 c, glm::vec3 h, glm::vec3 col, int mat) {
    glm::vec3 p[8];
    for (int i = 0; i < 8; ++i)
        p[i] = c + glm::vec3((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
    glm::vec2 s(std::max(h.x, h.z) * 2.f, h.y * 2.f);
    flatQuad(m, p[0], p[1], p[3], p[2], col, mat, {0, 0, -1}, s);
    flatQuad(m, p[4], p[5], p[7], p[6], col, mat, {0, 0, 1}, s);
    flatQuad(m, p[0], p[4], p[6], p[2], col, mat, {-1, 0, 0}, s);
    flatQuad(m, p[1], p[5], p[7], p[3], col, mat, {1, 0, 0}, s);
    flatQuad(m, p[2], p[3], p[7], p[6], col, mat, {0, 1, 0}, s);
    flatQuad(m, p[0], p[1], p[5], p[4], col, mat, {0, -1, 0}, s);
}

void cylinder(MeshData& m, glm::vec3 a, glm::vec3 b, float r0, float r1, int sides, glm::vec3 col, int mat,
              bool caps = false) {
    glm::vec3 axis = b - a;
    float len = glm::length(axis);
    glm::vec3 d = axis / len;
    glm::vec3 t = std::fabs(d.y) < 0.9f ? glm::normalize(glm::cross(d, glm::vec3(0, 1, 0)))
                                         : glm::normalize(glm::cross(d, glm::vec3(1, 0, 0)));
    glm::vec3 bt = glm::cross(d, t);
    uint32_t base = (uint32_t)m.verts.size();
    for (int i = 0; i <= sides; ++i) {
        float ang = 2.f * PI * i / sides;
        glm::vec3 n = std::cos(ang) * t + std::sin(ang) * bt;
        float u = (float)i / sides;
        m.add(V(a + n * r0, {u * 2.f * PI * r0, 0.f}, col, mat, n));
        m.add(V(b + n * r1, {u * 2.f * PI * r0, len * 0.5f}, col, mat, n));
    }
    for (int i = 0; i < sides; ++i) {
        uint32_t i0 = base + i * 2, i1 = i0 + 1, i2 = i0 + 3, i3 = i0 + 2;
        orientQuad(m, i0, i1, i2, i3, m.verts[i0].normal);
    }
    if (caps) {
        for (int e = 0; e < 2; ++e) {
            glm::vec3 c = e ? b : a;
            float r = e ? r1 : r0;
            glm::vec3 n = e ? d : -d;
            uint32_t ci = m.add(V(c, {0.5f, 0.5f}, col, mat, n));
            uint32_t first = (uint32_t)m.verts.size();
            for (int i = 0; i <= sides; ++i) {
                float ang = 2.f * PI * i / sides;
                glm::vec3 o = std::cos(ang) * t + std::sin(ang) * bt;
                m.add(V(c + o * r, glm::vec2(0.5f) + 0.5f * glm::vec2(std::cos(ang), std::sin(ang)), col, mat, n));
            }
            for (int i = 0; i < sides; ++i) {
                uint32_t p0 = first + i, p1 = first + i + 1;
                glm::vec3 fn = glm::cross(m.verts[p0].pos - c, m.verts[p1].pos - c);
                if (glm::dot(fn, n) >= 0) m.tri(ci, p0, p1); else m.tri(ci, p1, p0);
            }
        }
    }
}

void line(MeshData& m, glm::vec3 a, glm::vec3 b, glm::vec3 col = glm::vec3(0.07f, 0.055f, 0.04f)) {
    uint32_t i0 = m.add(V(a, {0, 0}, col, MAT_ROPE));
    uint32_t i1 = m.add(V(b, {1, 0}, col, MAT_ROPE));
    m.idx.push_back(i0);
    m.idx.push_back(i1);
}

// A cambered square sail hanging from a yard. Bulge toward +Z (wind from astern).
void sail(MeshData& m, glm::vec3 topCenter, float topW, glm::vec3 botCenter, float botW, glm::vec3 col,
          float bulge) {
    const int N = 12;
    uint32_t base = (uint32_t)m.verts.size();
    for (int j = 0; j <= N; ++j) {
        float v = (float)j / N;
        for (int i = 0; i <= N; ++i) {
            float u = (float)i / N;
            float w = glm::mix(topW, botW, v);
            glm::vec3 c = glm::mix(topCenter, botCenter, v);
            float b = bulge * std::sin(PI * u) * std::pow(std::sin(PI * (0.08f + 0.84f * v)), 0.8f);
            glm::vec3 p = c + glm::vec3((u - 0.5f) * w, 0.f, b);
            m.add(V(p, {u, v}, col, MAT_CANVAS));
        }
    }
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
            uint32_t a = base + j * (N + 1) + i;
            orientQuad(m, a, a + 1, a + N + 2, a + N + 1, {0, 0, 1});
        }
}

void tri3(MeshData& m, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 col, int mat) {
    glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
    uint32_t i0 = m.add(V(a, {0, 0}, col, mat, n));
    uint32_t i1 = m.add(V(b, {1, 0}, col, mat, n));
    uint32_t i2 = m.add(V(c, {0, 1}, col, mat, n));
    m.tri(i0, i1, i2);
}
}  // namespace

// ---------------- Hull profile ----------------
float HullProfile::halfBeam(float z) const {
    float u = glm::clamp(z / (L * 0.5f), -1.f, 1.f);
    if (u >= 0) return B * 0.5f * std::pow(std::max(0.f, 1.f - std::pow(u, 2.4f)), 0.55f);
    return B * 0.5f * (1.f - 0.3f * std::pow(-u, 3.f));
}
float HullProfile::railHeight(float z) const {
    float u = glm::clamp(z / (L * 0.5f), -1.f, 1.f);
    return 2.4f + 0.7f * u * u + 2.4f * sm(-0.46f, -0.51f, u) + 0.6f * sm(-0.8f, -1.0f, u) +
           1.3f * sm(0.52f, 0.57f, u);
}
float HullProfile::deckHeight(float z) const { return railHeight(z) - 1.1f; }
float HullProfile::keelDepth(float z) const {
    float u = glm::clamp(z / (L * 0.5f), -1.f, 1.f);
    return -draft * (1.f - 0.8f * sm(0.55f, 1.0f, u)) * (1.f - 0.15f * sm(-0.8f, -1.f, u));
}
float HullProfile::sectionWidth(float t) const {
    const float tb = 0.5f;
    if (t < tb) return 0.12f + 0.88f * std::pow(std::sin(t / tb * PI * 0.5f), 0.7f);
    float f = (t - tb) / (1.f - tb);
    return 1.f - 0.14f * f * f;
}
float DeckOutline::halfBeam(float z) const {
    float keel = hull.keelDepth(z), rail = hull.railHeight(z), deck = hull.deckHeight(z);
    float t = (deck - keel) / (rail - keel);
    return std::max(0.f, hull.halfBeam(z) * hull.sectionWidth(t) - 0.35f);
}

// ---------------- Galleon builder ----------------
GalleonGeometry buildGalleonGeometry(const GalleonParams& P) {
    GalleonGeometry G;
    HullProfile H{P.length, P.beam, P.draft};
    G.profile = H;
    G.outline.hull = H;
    G.outline.zMin = -P.length * 0.5f + 0.8f;
    G.outline.zMax = P.length * 0.5f * 0.86f;
    const float L = P.length, halfL = L * 0.5f;
    const glm::vec3 below(0.16f, 0.11f, 0.07f), hullC = P.hullColor, trim = P.trimColor;
    const glm::vec3 woodC(0.42f, 0.30f, 0.19f), darkWood(0.22f, 0.15f, 0.09f), iron(0.05f, 0.05f, 0.055f);
    const glm::vec3 rope(0.07f, 0.055f, 0.04f);

    // ---- Hull shell (both sides) ----
    const int NZ = 64, NT = 22;
    for (int side = -1; side <= 1; side += 2) {
        uint32_t base = (uint32_t)G.hull.verts.size();
        for (int i = 0; i <= NZ; ++i) {
            float z = -halfL + L * i / NZ;
            float hb = H.halfBeam(z), keel = H.keelDepth(z), rail = H.railHeight(z), deck = H.deckHeight(z);
            for (int j = 0; j <= NT; ++j) {
                float t = (float)j / NT;
                float y = glm::mix(keel, rail, t);
                float x = side * hb * H.sectionWidth(t);
                glm::vec3 c = hullC;
                if (y < 0.25f) c = below;
                else if (std::fabs(y - (deck - 0.15f)) < 0.18f || y > rail - 0.22f) c = trim;
                else if (std::fabs(y - 0.45f) < 0.12f) c = trim * 0.8f;
                G.hull.add(V({x, y, z}, {z * 0.25f, y * 0.8f}, c, MAT_WOOD));
            }
        }
        for (int i = 0; i < NZ; ++i)
            for (int j = 0; j < NT; ++j) {
                uint32_t a = base + i * (NT + 1) + j;
                orientQuad(G.hull, a, a + 1, a + NT + 2, a + NT + 1, {(float)side, -0.3f, 0.f});
            }
    }
    G.hull.recomputeNormals();
    // Normals at the keel seam / bow stem are averaged per side only, which keeps a crisp keel line.

    MeshData flat;  // flat-shaded hull details appended after normal recompute
    // ---- Inner bulwarks + rail caps ----
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < NZ; ++i) {
            float z0 = -halfL + L * i / NZ, z1 = -halfL + L * (i + 1) / NZ;
            auto outerX = [&](float z) { return H.halfBeam(z) * H.sectionWidth(1.f); };
            glm::vec3 o0(side * outerX(z0), H.railHeight(z0), z0), o1(side * outerX(z1), H.railHeight(z1), z1);
            glm::vec3 in0(side * std::max(0.f, outerX(z0) - 0.3f), o0.y, z0),
                in1(side * std::max(0.f, outerX(z1) - 0.3f), o1.y, z1);
            glm::vec3 d0(in0.x, H.deckHeight(z0), z0), d1(in1.x, H.deckHeight(z1), z1);
            flatQuad(flat, in0, in1, d1, d0, hullC * 1.6f + glm::vec3(0.05f), MAT_WOOD, {(float)-side, 0, 0},
                     {0.5f, 0.6f});
            flatQuad(flat, o0, o1, in1, in0, trim, MAT_WOOD, {0, 1, 0}, {0.5f, 0.1f});
        }
    }
    // ---- Transom (stern face) ----
    {
        float z = -halfL;
        float keel = H.keelDepth(z), rail = H.railHeight(z), hb = H.halfBeam(z);
        for (int j = 0; j < NT; ++j) {
            float t0 = (float)j / NT, t1 = (float)(j + 1) / NT;
            float y0 = glm::mix(keel, rail, t0), y1 = glm::mix(keel, rail, t1);
            float x0 = hb * H.sectionWidth(t0), x1 = hb * H.sectionWidth(t1);
            glm::vec3 c = y0 < 0.25f ? below : hullC;
            flatQuad(flat, {-x0, y0, z}, {x0, y0, z}, {x1, y1, z}, {-x1, y1, z}, c, MAT_WOOD, {0, 0, -1},
                     {x0 * 0.5f, 0.3f});
        }
        // Stern gallery windows: two rows of warm glowing panes with dark frames.
        float deckQ = H.deckHeight(z);
        for (int row = 0; row < 2; ++row) {
            float wy = deckQ - 1.6f + row * 1.5f;
            for (int k = -2; k <= 2; ++k) {
                float wx = k * 1.1f;
                glm::vec3 glow = glm::vec3(3.2f, 1.7f, 0.6f) * (row ? 0.8f : 1.0f);
                flatQuad(flat, {wx - 0.38f, wy, z - 0.06f}, {wx + 0.38f, wy, z - 0.06f}, {wx + 0.38f, wy + 0.8f, z - 0.06f},
                         {wx - 0.38f, wy + 0.8f, z - 0.06f}, glow, MAT_EMISSIVE, {0, 0, -1});
                box(flat, {wx, wy + 0.4f, z - 0.08f}, {0.03f, 0.42f, 0.03f}, darkWood, MAT_PLAIN);
            }
            box(flat, {0, wy - 0.06f, z - 0.12f}, {3.0f, 0.06f, 0.12f}, trim, MAT_WOOD);
            box(flat, {0, wy + 0.86f, z - 0.12f}, {3.0f, 0.06f, 0.12f}, trim, MAT_WOOD);
        }
    }
    // ---- Gun ports along both sides ----
    for (int side = -1; side <= 1; side += 2) {
        for (int k = 0; k < 9; ++k) {
            float z = -0.30f * L + k * (0.62f * L / 8.f);
            float y = 0.95f;
            float keel = H.keelDepth(z), rail = H.railHeight(z);
            float t = (y - keel) / (rail - keel);
            float x = side * (H.halfBeam(z) * H.sectionWidth(t) + 0.04f);
            flatQuad(flat, {x, y - 0.3f, z - 0.35f}, {x, y - 0.3f, z + 0.35f}, {x, y + 0.3f, z + 0.35f},
                     {x, y + 0.3f, z - 0.35f}, glm::vec3(0.012f), MAT_PLAIN, {(float)side, 0, 0});
            // red-painted port lid propped open above
            flatQuad(flat, {x, y + 0.32f, z - 0.38f}, {x, y + 0.32f, z + 0.38f},
                     {x + side * 0.45f, y + 0.85f, z + 0.38f}, {x + side * 0.45f, y + 0.85f, z - 0.38f},
                     glm::vec3(0.35f, 0.06f, 0.04f), MAT_WOOD, {(float)side, -1, 0}, {0.3f, 0.3f});
            // cannon muzzle poking out
            cylinder(flat, {x - side * 0.1f, y, z}, {x + side * 0.7f, y, z}, 0.16f, 0.13f, 8, iron, MAT_PLAIN);
        }
    }
    G.hull.append(flat);

    // ---- Deck ----
    {
        const int DZ = 80, DX = 8;
        for (int i = 0; i <= DZ; ++i) {
            float z = -halfL + L * i / DZ;
            float hw = std::max(0.f, H.halfBeam(z) * H.sectionWidth(1.f) - 0.3f);
            float y = H.deckHeight(z);
            for (int j = 0; j <= DX; ++j) {
                float x = -hw + 2.f * hw * j / DX;
                G.deck.add(V({x, y, z}, {z * 0.25f, x * 0.8f}, glm::vec3(0.52f, 0.42f, 0.30f), MAT_DECK));
            }
        }
        for (int i = 0; i < DZ; ++i)
            for (int j = 0; j < DX; ++j) {
                uint32_t a = i * (DX + 1) + j;
                orientQuad(G.deck, a, a + 1, a + DX + 2, a + DX + 1, {0, 1, 0});
            }
        G.deck.recomputeNormals();
    }
    // Quarterdeck & forecastle front bulkheads with lit doorways.
    {
        float zq = -0.485f * halfL;
        float lo = H.deckHeight(zq + 1.f), hi = H.deckHeight(zq - 1.f), hw = H.halfBeam(zq) - 0.4f;
        flatQuad(G.wood, {-hw, lo, zq + 0.45f}, {hw, lo, zq + 0.45f}, {hw, hi, zq + 0.45f}, {-hw, hi, zq + 0.45f},
                 hullC * 1.8f, MAT_WOOD, {0, 0, 1}, {hw * 0.5f, 0.8f});
        for (int k = -1; k <= 1; k += 2)
            flatQuad(G.wood, {k * 1.6f - 0.45f, lo + 0.1f, zq + 0.5f}, {k * 1.6f + 0.45f, lo + 0.1f, zq + 0.5f},
                     {k * 1.6f + 0.45f, lo + 1.9f, zq + 0.5f}, {k * 1.6f - 0.45f, lo + 1.9f, zq + 0.5f},
                     glm::vec3(1.6f, 0.85f, 0.3f), MAT_EMISSIVE, {0, 0, 1});
        // rail along the quarterdeck edge
        cylinder(G.wood, {-hw, hi + 1.0f, zq + 0.3f}, {hw, hi + 1.0f, zq + 0.3f}, 0.07f, 0.07f, 6, trim, MAT_WOOD);
        for (int k = 0; k <= 10; ++k) {
            float x = -hw + 2.f * hw * k / 10.f;
            cylinder(G.wood, {x, hi, zq + 0.3f}, {x, hi + 1.0f, zq + 0.3f}, 0.05f, 0.05f, 6, trim, MAT_WOOD);
        }
        float zf = 0.545f * halfL;
        float flo = H.deckHeight(zf - 1.f), fhi = H.deckHeight(zf + 1.f), fw = H.halfBeam(zf) - 0.4f;
        flatQuad(G.wood, {-fw, flo, zf - 0.6f}, {fw, flo, zf - 0.6f}, {fw, fhi, zf - 0.6f}, {-fw, fhi, zf - 0.6f},
                 hullC * 1.8f, MAT_WOOD, {0, 0, -1}, {fw * 0.5f, 0.5f});
    }

    // ---- Masts, tops, yards, sails ----
    struct MastDef { float z, height; int yards; float yardScale; bool main; };
    MastDef masts[3] = {{0.30f * L, 25.f, 3, 0.9f, false}, {-0.02f * L, 30.f, 3, 1.0f, true}, {-0.33f * L, 19.f, 2, 0.7f, false}};
    glm::vec3 mastTops[3];
    float yardHeights[3][3];
    for (int mi = 0; mi < 3; ++mi) {
        const MastDef& M = masts[mi];
        float deckY = H.deckHeight(M.z);
        glm::vec3 base(0, deckY - 0.2f, M.z), top(0, deckY + M.height, M.z);
        mastTops[mi] = top;
        cylinder(G.wood, base, top, 0.38f, 0.16f, 12, woodC, MAT_WOOD);
        // fighting top platform
        float topY = deckY + M.height * 0.56f;
        cylinder(G.wood, {0, topY, M.z}, {0, topY + 0.25f, M.z}, 1.7f, 1.7f, 12, darkWood, MAT_WOOD, true);
        // capstan-like mast coat
        cylinder(G.wood, {0, deckY - 0.05f, M.z}, {0, deckY + 0.5f, M.z}, 0.65f, 0.5f, 12, darkWood, MAT_WOOD, true);
        static const float fracs[3] = {0.36f, 0.64f, 0.9f};
        static const float lens[3] = {2.0f, 1.55f, 1.05f};
        MeshData& sm_ = M.main ? G.mainSails : G.sails;
        for (int y = 0; y < M.yards; ++y) {
            float yy = deckY + M.height * fracs[y];
            float len = P.beam * lens[y] * M.yardScale;
            yardHeights[mi][y] = yy;
            cylinder(G.wood, {-len * 0.5f, yy, M.z + 0.45f}, {len * 0.5f, yy, M.z + 0.45f}, 0.17f, 0.17f, 8, darkWood,
                     MAT_WOOD);
            // sail below this yard
            float botY = (y == 0) ? deckY + 3.2f : yardHeights[mi][y - 1] + 0.3f;
            float botW = (y == 0) ? len * 1.0f : P.beam * lens[y - 1] * M.yardScale * 0.92f;
            sail(sm_, {0, yy - 0.15f, M.z + 0.55f}, len * 0.92f, {0, botY, M.z + 0.55f}, botW,
                 P.sailColor * (0.95f + 0.05f * y), 0.10f * len);
            // yard lifts
            line(G.rigging, {-len * 0.5f, yy, M.z + 0.45f}, {0, yy + 2.5f, M.z});
            line(G.rigging, {len * 0.5f, yy, M.z + 0.45f}, {0, yy + 2.5f, M.z});
            // sheets from the sail's lower corners to the rail
            if (y == 0) {
                float rz = M.z - 4.f;
                float rx = H.halfBeam(rz) - 0.2f;
                line(G.rigging, {-botW * 0.5f, botY, M.z + 0.6f}, {-rx, H.railHeight(rz), rz});
                line(G.rigging, {botW * 0.5f, botY, M.z + 0.6f}, {rx, H.railHeight(rz), rz});
            }
        }
        // shrouds + ratlines
        for (int side = -1; side <= 1; side += 2) {
            std::vector<std::pair<glm::vec3, glm::vec3>> shrouds;
            for (int k = 0; k < 5; ++k) {
                float rz = M.z - 1.8f + k * 0.8f;
                glm::vec3 lo(side * (H.halfBeam(rz) * H.sectionWidth(1.f) + 0.1f), H.railHeight(rz) - 0.4f, rz);
                glm::vec3 hiP(side * 0.3f, topY, M.z);
                line(G.rigging, lo, hiP);
                shrouds.push_back({lo, hiP});
                // topmast shrouds from top edge to masthead
                if (k % 2 == 0) line(G.rigging, {side * 1.6f, topY + 0.25f, M.z - 0.8f + k * 0.4f}, top - glm::vec3(0, 1.f, 0));
            }
            for (int r = 1; r < 14; ++r) {
                float f = r / 14.f;
                for (size_t k = 0; k + 1 < shrouds.size(); ++k)
                    line(G.rigging, glm::mix(shrouds[k].first, shrouds[k].second, f),
                         glm::mix(shrouds[k + 1].first, shrouds[k + 1].second, f), rope);
            }
            // chain-plate channel on the hull
            float cz = M.z - 0.2f;
            box(G.wood, {side * (H.halfBeam(cz) * H.sectionWidth(1.f) + 0.12f), H.railHeight(cz) - 0.45f, cz},
                {0.15f, 0.06f, 2.0f}, darkWood, MAT_WOOD);
        }
        if (M.main) {
            G.mainMastTop = top;
            G.mainMastBase = glm::vec3(0, deckY, M.z);
            // long pennant streaming aft
            glm::vec3 fp = top + glm::vec3(0, 0.2f, 0);
            tri3(G.wood, fp, fp + glm::vec3(0, -0.7f, 0), fp + glm::vec3(0.0f, -0.3f, -6.f), glm::vec3(0.45f, 0.04f, 0.03f), MAT_PLAIN);
            tri3(G.wood, fp, fp + glm::vec3(0.0f, -0.3f, -6.f), fp + glm::vec3(0, -0.7f, 0), glm::vec3(0.45f, 0.04f, 0.03f), MAT_PLAIN);
        }
        // masthead truck + lantern-less cap
        cylinder(G.wood, top, top + glm::vec3(0, 0.3f, 0), 0.25f, 0.25f, 8, darkWood, MAT_WOOD, true);
    }
    // spanker gaff + sail on mizzen
    {
        float z = masts[2].z, dy = H.deckHeight(z);
        glm::vec3 gaffA(0, dy + 12.f, z - 0.3f), gaffB(0, dy + 14.f, z - 8.5f);
        glm::vec3 boomA(0, dy + 2.6f, z - 0.3f), boomB(0, dy + 2.6f, z - 9.5f);
        cylinder(G.wood, gaffA, gaffB, 0.14f, 0.1f, 8, darkWood, MAT_WOOD);
        cylinder(G.wood, boomA, boomB, 0.15f, 0.12f, 8, darkWood, MAT_WOOD);
        const int N = 10;
        uint32_t base = (uint32_t)G.sails.verts.size();
        for (int j = 0; j <= N; ++j)
            for (int i = 0; i <= N; ++i) {
                float u = (float)i / N, v = (float)j / N;
                glm::vec3 p = glm::mix(glm::mix(gaffA, gaffB, u), glm::mix(boomA, boomB, u), v);
                p.x += 0.8f * std::sin(PI * u) * std::sin(PI * v);
                G.sails.add(V(p, {u, v}, P.sailColor * 0.92f, MAT_CANVAS));
            }
        for (int j = 0; j < N; ++j)
            for (int i = 0; i < N; ++i) {
                uint32_t a = base + j * (N + 1) + i;
                orientQuad(G.sails, a, a + 1, a + N + 2, a + N + 1, {1, 0, 0});
            }
    }
    // bowsprit, jib sails, stays
    glm::vec3 bsBase(0, H.railHeight(halfL - 1.f) - 0.4f, halfL - 1.5f);
    glm::vec3 bsTip = bsBase + glm::normalize(glm::vec3(0, 0.42f, 1.f)) * 14.f;
    cylinder(G.wood, bsBase, bsTip, 0.32f, 0.12f, 10, woodC, MAT_WOOD);
    for (int k = 0; k < 2; ++k) {
        glm::vec3 a = glm::mix(bsBase, bsTip, 0.55f + 0.42f * k);
        glm::vec3 headP = mastTops[0] - glm::vec3(0, 3.f + 5.f * k, 0);
        glm::vec3 foot = glm::mix(bsBase, bsTip, 0.15f + 0.3f * k) + glm::vec3(0, 1.2f, 0);
        // subdivided triangle so the billow shader can move it
        const int N = 8;
        uint32_t base = (uint32_t)G.sails.verts.size();
        for (int j = 0; j <= N; ++j)
            for (int i = 0; i <= N - j; ++i) {
                float u = (float)i / N, v = (float)j / N;
                glm::vec3 p = a + (headP - a) * u + (foot - a) * v;
                p.x += 0.6f * std::sin(PI * std::min(1.f, u + v)) * (1.f - v);
                G.sails.add(V(p, {u, v}, P.sailColor * 0.9f, MAT_CANVAS));
            }
        auto id = [&](int i, int j) { int off = 0; for (int r = 0; r < j; ++r) off += N + 1 - r; return base + off + i; };
        for (int j = 0; j < N; ++j)
            for (int i = 0; i < N - j; ++i) {
                G.sails.tri(id(i, j), id(i + 1, j), id(i, j + 1));
                if (i + 1 <= N - j - 1) G.sails.tri(id(i + 1, j), id(i + 1, j + 1), id(i, j + 1));
            }
        line(G.rigging, a, headP);
    }
    line(G.rigging, bsTip, mastTops[0]);
    line(G.rigging, mastTops[1], mastTops[0] - glm::vec3(0, 9.f, 0));
    line(G.rigging, mastTops[2], mastTops[1] - glm::vec3(0, 12.f, 0));
    line(G.rigging, bsTip, bsBase + glm::vec3(0, -2.5f, 0.5f));  // bobstay
    for (int side = -1; side <= 1; side += 2) {
        for (int mi = 0; mi < 3; ++mi) {
            float rz = masts[mi].z - 7.f;
            line(G.rigging, mastTops[mi] - glm::vec3(0, 0.5f, 0),
                 {side * (H.halfBeam(rz) - 0.1f), H.railHeight(rz), rz});
        }
    }

    // ---- Deck furniture ----
    {
        // ship's wheel on the quarterdeck
        float wz = -0.40f * L, wy = H.deckHeight(wz);
        glm::vec3 c(0, wy + 1.25f, wz);
        cylinder(G.wood, {0, wy, wz - 0.3f}, {0, wy + 1.2f, wz - 0.3f}, 0.18f, 0.15f, 8, darkWood, MAT_WOOD, true);
        cylinder(G.wood, c + glm::vec3(0, 0, -0.35f), c + glm::vec3(0, 0, 0.1f), 0.08f, 0.08f, 8, iron, MAT_PLAIN);
        const int seg = 16;
        for (int k = 0; k < seg; ++k) {
            float a0 = 2 * PI * k / seg, a1 = 2 * PI * (k + 1) / seg;
            cylinder(G.wood, c + 0.75f * glm::vec3(std::cos(a0), std::sin(a0), 0),
                     c + 0.75f * glm::vec3(std::cos(a1), std::sin(a1), 0), 0.05f, 0.05f, 6, woodC * 1.2f, MAT_WOOD);
        }
        for (int k = 0; k < 8; ++k) {
            float a = 2 * PI * k / 8;
            glm::vec3 d(std::cos(a), std::sin(a), 0);
            cylinder(G.wood, c, c + d * 1.0f, 0.04f, 0.035f, 6, woodC * 1.2f, MAT_WOOD);
        }
        // binnacle
        box(G.wood, {0, wy + 0.55f, wz + 1.2f}, {0.3f, 0.55f, 0.3f}, darkWood, MAT_WOOD);
        // hatches with gratings
        for (float hz : {-0.12f * L, 0.12f * L}) {
            float hy = H.deckHeight(hz);
            box(G.wood, {0, hy + 0.2f, hz}, {1.1f, 0.2f, 1.5f}, darkWood, MAT_WOOD);
            for (int k = -4; k <= 4; ++k)
                box(G.wood, {k * 0.24f, hy + 0.42f, hz}, {0.04f, 0.03f, 1.45f}, glm::vec3(0.05f), MAT_PLAIN);
        }
        // capstan
        float cz = 0.20f * L, cy = H.deckHeight(cz);
        cylinder(G.wood, {0, cy, cz}, {0, cy + 1.0f, cz}, 0.55f, 0.45f, 12, woodC, MAT_WOOD, true);
        for (int k = 0; k < 6; ++k) {
            float a = 2 * PI * k / 6;
            cylinder(G.wood, {0, cy + 0.85f, cz}, {1.3f * std::cos(a), cy + 0.85f, cz + 1.3f * std::sin(a)}, 0.05f,
                     0.05f, 6, woodC, MAT_WOOD);
        }
        // barrels and crates along the bulwarks
        for (int side = -1; side <= 1; side += 2)
            for (int k = 0; k < 3; ++k) {
                float bz = -0.2f * L + k * 1.0f + (side > 0 ? 7.f : 0.f);
                float bx = side * (G.outline.halfBeam(bz) - 0.45f), by = H.deckHeight(bz);
                cylinder(G.wood, {bx, by, bz}, {bx, by + 0.95f, bz}, 0.36f, 0.36f, 12, woodC * 0.8f, MAT_WOOD, true);
                cylinder(G.wood, {bx, by + 0.18f, bz}, {bx, by + 0.24f, bz}, 0.375f, 0.375f, 12, iron, MAT_PLAIN);
                cylinder(G.wood, {bx, by + 0.72f, bz}, {bx, by + 0.78f, bz}, 0.375f, 0.375f, 12, iron, MAT_PLAIN);
            }
        box(G.wood, {-2.0f, H.deckHeight(0.05f * L) + 0.35f, 0.05f * L}, {0.5f, 0.35f, 0.6f}, darkWood, MAT_WOOD);
        // deck guns on carriages aimed through the bulwarks
        for (int side = -1; side <= 1; side += 2)
            for (int k = 0; k < 3; ++k) {
                float gz = -0.05f * L + k * 3.5f;
                float gx = side * (G.outline.halfBeam(gz) - 0.9f), gy = H.deckHeight(gz);
                box(G.wood, {gx, gy + 0.25f, gz}, {0.5f, 0.25f, 0.4f}, darkWood, MAT_WOOD);
                cylinder(G.wood, {gx - side * 0.5f, gy + 0.6f, gz}, {gx + side * 1.3f, gy + 0.65f, gz}, 0.22f, 0.15f,
                         10, iron, MAT_PLAIN);
            }
    }

    // ---- Lanterns (emissive cages; also become point lights) ----
    auto lantern = [&](glm::vec3 p) {
        box(G.wood, p, {0.18f, 0.25f, 0.18f}, glm::vec3(5.0f, 2.6f, 0.9f), MAT_EMISSIVE);
        box(G.wood, p + glm::vec3(0, 0.3f, 0), {0.22f, 0.05f, 0.22f}, iron, MAT_PLAIN);
        G.lanternLocal.push_back(p);
    };
    float sternRail = H.railHeight(-halfL);
    lantern({0, sternRail + 1.6f, -halfL - 0.4f});
    lantern({-2.6f, sternRail + 0.9f, -halfL - 0.3f});
    lantern({2.6f, sternRail + 0.9f, -halfL - 0.3f});
    cylinder(G.wood, {0, sternRail, -halfL - 0.3f}, {0, sternRail + 1.3f, -halfL - 0.4f}, 0.06f, 0.06f, 6, iron, MAT_PLAIN);
    // waist lanterns on posts atop the bulwark: light the deck without flooding the sails above
    for (int side = -1; side <= 1; side += 2)
        for (float lz : {-0.16f * L, 0.18f * L}) {
            float x = side * (H.halfBeam(lz) * H.sectionWidth(1.f) - 0.15f);
            cylinder(G.wood, {x, H.railHeight(lz), lz}, {x, H.railHeight(lz) + 0.7f, lz}, 0.05f, 0.05f, 6, iron, MAT_PLAIN);
            lantern({x, H.railHeight(lz) + 0.95f, lz});
        }
    lantern({0, H.railHeight(halfL * 0.8f) + 0.6f, halfL * 0.8f});

    return G;
}

// ---------------- Pose ----------------
glm::vec3 shipForward(float heading) { return glm::vec3(std::sin(heading), 0.f, std::cos(heading)); }

glm::mat4 shipPose(const ShipState& s, float t, float length, float beam) {
    glm::vec3 f = shipForward(s.heading);
    glm::vec2 fw(f.x, f.z), right(f.z, -f.x);
    float hb = waveHeight(s.xz + fw * length * 0.4f, t);
    float hs = waveHeight(s.xz - fw * length * 0.4f, t);
    float hr = waveHeight(s.xz + right * beam * 0.5f, t);
    float hl = waveHeight(s.xz - right * beam * 0.5f, t);
    float hc = waveHeight(s.xz, t);
    float y = 0.5f * hc + 0.125f * (hb + hs + hr + hl);
    float pitch = -std::atan2(hb - hs, length * 0.8f);
    float roll = std::atan2(hr - hl, beam);
    // gentle extra roll from wind heel
    roll += 0.05f;
    float sp = glm::clamp(s.sinkProgress, 0.f, 1.f);
    if (sp > 0.f) {
        y -= 26.f * std::pow(sp, 1.6f);
        pitch += 0.75f * sm(0.f, 0.55f, sp);   // bow dips, stern rises
        roll += 0.35f * sm(0.f, 0.4f, sp);
    }
    glm::mat4 m = glm::translate(glm::mat4(1.f), glm::vec3(s.xz.x, y, s.xz.y));
    m = glm::rotate(m, s.heading, glm::vec3(0, 1, 0));
    m = glm::rotate(m, pitch, glm::vec3(1, 0, 0));
    m = glm::rotate(m, roll, glm::vec3(0, 0, 1));
    return m;
}
