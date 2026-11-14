// Assert-style tests for the GL-free core (waves, ship pose, deck clamp, lightning, event).
#include "camera.hpp"
#include "event.hpp"
#include "ocean_core.hpp"
#include "ship_core.hpp"
#include "weather_core.hpp"
#include <cmath>
#include <cstdio>
#include <functional>
#include <random>
#include <string>
#include <vector>

static int g_fail = 0, g_checks = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        ++g_checks;                                                                         \
        if (!(cond)) { ++g_fail; std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

struct T { const char* name; std::function<void()> fn; };
static std::vector<T>& reg() { static std::vector<T> r; return r; }
struct Reg { Reg(const char* n, std::function<void()> f) { reg().push_back({n, f}); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()

static float ampSum() { float s = 0; for (auto& w : oceanWaves()) s += w.amplitude; return s; }

// ---------------- Waves ----------------
TEST(wave_height_bounded) {
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> d(-500.f, 500.f), tt(0.f, 300.f);
    float lim = ampSum() + 1e-3f;
    for (int i = 0; i < 1000; ++i) CHECK(std::fabs(waveHeight({d(rng), d(rng)}, tt(rng))) <= lim);
}
TEST(wave_normal_unit) {
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> d(-300.f, 300.f);
    for (int i = 0; i < 500; ++i) {
        WaveSample s = waveSample({d(rng), d(rng)}, 12.5f);
        CHECK(std::fabs(glm::length(s.normal) - 1.f) < 1e-3f);
        CHECK(s.normal.y > 0.3f);
    }
}
TEST(wave_time_continuity) {
    for (int i = 0; i < 200; ++i) {
        glm::vec2 p(i * 3.1f - 300.f, i * -1.7f + 50.f);
        CHECK(std::fabs(waveHeight(p, 10.f) - waveHeight(p, 10.001f)) < 0.05f);
    }
}
TEST(wave_large_time_ok) {
    float h = waveHeight({12.f, -40.f}, 4000.f);
    CHECK(std::isfinite(h));
    CHECK(std::fabs(h) <= ampSum() + 1e-3f);
}
TEST(wave_height_inverts_horizontal_displacement) {
    // The displaced point found by waveSample must land (horizontally) on the query xz.
    for (int i = 0; i < 100; ++i) {
        glm::vec2 p(i * 7.3f - 350.f, i * 2.9f - 120.f);
        WaveSample s = waveSample(p, 33.f);
        CHECK(std::fabs(s.displaced.x - p.x) < 0.05f);
        CHECK(std::fabs(s.displaced.z - p.y) < 0.05f);
    }
}

// ---------------- Deck clamp ----------------
static DeckOutline testOutline() { return buildGalleonGeometry(GalleonParams{}).outline; }

TEST(clamp_inside_unchanged) {
    DeckOutline o = testOutline();
    glm::vec2 p(0.5f, 0.f);
    glm::vec2 r = clampToDeck(p, o);
    CHECK(std::fabs(r.x - p.x) < 1e-5f && std::fabs(r.y - p.y) < 1e-5f);
}
TEST(clamp_outside_beam_pulls_in) {
    DeckOutline o = testOutline();
    glm::vec2 r = clampToDeck({50.f, 0.f}, o);
    CHECK(std::fabs(r.x - (o.halfBeam(0.f) - 0.4f)) < 1e-4f);
    glm::vec2 l = clampToDeck({-50.f, 0.f}, o);
    CHECK(std::fabs(l.x + (o.halfBeam(0.f) - 0.4f)) < 1e-4f);
}
TEST(clamp_past_bow_pulls_in) {
    DeckOutline o = testOutline();
    glm::vec2 r = clampToDeck({0.f, 500.f}, o);
    CHECK(r.y <= o.zMax - 0.4f + 1e-4f);
    glm::vec2 s = clampToDeck({0.f, -500.f}, o);
    CHECK(s.y >= o.zMin + 0.4f - 1e-4f);
}
TEST(clamp_handles_tapered_bow) {
    DeckOutline o = testOutline();
    for (float x : {-9.f, -1.f, 0.f, 3.f, 9.f}) {
        glm::vec2 r = clampToDeck({x, o.zMax - 0.1f}, o);
        float hb = o.halfBeam(r.y) - 0.4f;
        CHECK(r.x == 0.f || std::fabs(r.x) <= hb + 1e-4f);
    }
}

// ---------------- Galleon ----------------
static bool validMesh(const MeshData& m, int stride) {
    if (m.idx.empty() || m.idx.size() % stride) return false;
    for (uint32_t i : m.idx) if (i >= m.verts.size()) return false;
    return true;
}
TEST(galleon_nonempty) {
    GalleonGeometry g = buildGalleonGeometry(GalleonParams{});
    CHECK(validMesh(g.hull, 3));
    CHECK(validMesh(g.deck, 3));
    CHECK(validMesh(g.wood, 3));
    CHECK(validMesh(g.sails, 3));
    CHECK(validMesh(g.mainSails, 3));
    CHECK(validMesh(g.rigging, 2));
    CHECK(g.lanternLocal.size() >= 3);
}
TEST(galleon_bounds) {
    GalleonParams p;
    GalleonGeometry g = buildGalleonGeometry(p);
    glm::vec3 lo(1e9f), hi(-1e9f);
    for (auto& v : g.hull.verts) { lo = glm::min(lo, v.pos); hi = glm::max(hi, v.pos); }
    CHECK(hi.z - lo.z <= p.length * 1.1f);
    CHECK(hi.z - lo.z >= p.length * 0.9f);
    CHECK(hi.x - lo.x <= p.beam * 1.25f);  // includes gun-port lids & muzzles
    CHECK(lo.y < -p.draft * 0.9f);
}
TEST(galleon_normals_unit) {
    GalleonGeometry g = buildGalleonGeometry(GalleonParams{});
    for (const MeshData* m : {&g.hull, &g.deck, &g.wood, &g.sails})
        for (auto& v : m->verts) CHECK(std::fabs(glm::length(v.normal) - 1.f) < 1e-3f);
}
TEST(galleon_hull_normals_face_outward) {
    GalleonGeometry g = buildGalleonGeometry(GalleonParams{});
    int bad = 0, n = 0;
    for (auto& v : g.hull.verts) {
        // outer shell only: below deck level (inner bulwark faces start at deck height and face inward on purpose)
        if (std::fabs(v.pos.z) < 5.f && std::fabs(v.pos.x) > 3.f && v.mat == MAT_WOOD && v.pos.y < 1.0f) {
            ++n;
            if (v.normal.x * v.pos.x < 0.f) ++bad;
        }
    }
    CHECK(n > 20);
    CHECK(bad == 0);
}
TEST(deck_outline_matches_hull) {
    GalleonParams p;
    DeckOutline o = buildGalleonGeometry(p).outline;
    CHECK(std::fabs(o.halfBeam(0.f) - p.beam * 0.5f) < p.beam * 0.5f * 0.15f);
    CHECK(o.deckHeight(-14.f) > o.deckHeight(0.f) + 1.5f);  // raised quarterdeck
}

// ---------------- Pose ----------------
TEST(pose_follows_wave) {
    for (int i = 0; i < 50; ++i) {
        ShipState s;
        s.xz = glm::vec2(i * 11.f - 200.f, i * 5.f);
        s.heading = i * 0.4f;
        float t = i * 1.3f;
        glm::mat4 m = shipPose(s, t, 34.f, 9.f);
        // a ship averages over its hull, so allow some difference from the point height
        CHECK(std::fabs(m[3][1] - waveHeight(s.xz, t)) < 1.5f);
    }
}
TEST(pose_rotation_orthonormal) {
    ShipState s; s.xz = {40.f, -12.f}; s.heading = 1.1f;
    glm::mat4 m = shipPose(s, 17.f, 34.f, 9.f);
    glm::mat3 r(m);
    glm::mat3 i = glm::transpose(r) * r;
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b) CHECK(std::fabs(i[a][b] - (a == b ? 1.f : 0.f)) < 1e-4f);
}
TEST(pose_sinking_lowers) {
    ShipState s; s.sinkProgress = 1.f;
    CHECK(shipPose(s, 5.f, 34.f, 9.f)[3][1] < -20.f);
}
TEST(pose_heading_moves_bow) {
    ShipState s; s.heading = 0.f;
    glm::mat4 m = shipPose(s, 0.f, 34.f, 9.f);
    glm::vec3 bow = glm::vec3(m * glm::vec4(0, 0, 10, 1));
    CHECK(bow.z > 9.f);
}

// ---------------- Lightning ----------------
static bool near3(glm::vec3 a, glm::vec3 b) { return glm::length(a - b) < 1e-3f; }
TEST(bolt_connects_endpoints) {
    glm::vec3 from(10, 700, -30), to(-40, 0, 80);
    Bolt b = generateBolt(from, to, 42);
    CHECK(b.mainCount > 8);
    CHECK(near3(b.segments.front().a, from));
    CHECK(near3(b.segments[b.mainCount - 1].b, to));
    for (int i = 0; i + 1 < b.mainCount; ++i) CHECK(near3(b.segments[i].b, b.segments[i + 1].a));
}
TEST(bolt_deterministic) {
    Bolt a = generateBolt({0, 500, 0}, {0, 0, 0}, 9), b = generateBolt({0, 500, 0}, {0, 0, 0}, 9);
    CHECK(a.segments.size() == b.segments.size());
    for (size_t i = 0; i < a.segments.size() && i < b.segments.size(); ++i) CHECK(near3(a.segments[i].a, b.segments[i].a));
}
TEST(bolt_has_branches) {
    Bolt b = generateBolt({0, 700, 0}, {0, 0, 0}, 5);
    CHECK((int)b.segments.size() > b.mainCount);
}
TEST(flash_envelope_bounds) {
    for (float a = -1.f; a < 3.f; a += 0.01f) {
        float f = flashEnvelope(a);
        CHECK(f >= 0.f && f <= 1.f);
    }
    CHECK(flashEnvelope(2.f) == 0.f);
    CHECK(flashEnvelope(0.f) > 0.5f);
}

// ---------------- Event ----------------
TEST(phases_in_order) {
    StrikeEvent e; e.t0 = 20.f;
    CHECK(e.phase(19.f) == Phase::Calm);
    CHECK(e.phase(20.1f) == Phase::Strike);
    CHECK(e.phase(30.f) == Phase::Burning);
    CHECK(e.phase(70.f) == Phase::Sinking);
    CHECK(e.phase(120.f) == Phase::Sunk);
}
TEST(sink_progress_monotonic) {
    StrikeEvent e;
    float prev = -1.f;
    for (float t = 0.f; t < 150.f; t += 0.25f) {
        float s = e.sinkProgress(t);
        CHECK(s >= 0.f && s <= 1.f);
        CHECK(s >= prev);
        prev = s;
    }
    CHECK(e.sinkProgress(200.f) == 1.f);
}
TEST(retrigger_resets) {
    StrikeEvent e;
    float now = 100.f;
    e.retrigger(now);
    Phase p = e.phase(now);
    CHECK(p == Phase::Calm || p == Phase::Strike);
    CHECK(e.sinkProgress(now) == 0.f);
    CHECK(e.fireIntensity(now) == 0.f);
}
TEST(fire_fades_when_sunk) {
    StrikeEvent e;
    CHECK(e.fireIntensity(e.t0 + 30.f) > 0.9f);
    CHECK(e.fireIntensity(e.t0 + 95.f) == 0.f);
}

int main(int argc, char** argv) {
    std::string only = argc > 1 ? argv[1] : "";
    for (auto& t : reg()) {
        if (!only.empty() && only != t.name) continue;
        int before = g_fail;
        t.fn();
        std::printf("%s %s\n", g_fail == before ? "[ ok ]" : "[FAIL]", t.name);
    }
    std::printf("%d checks, %d failures\n", g_checks, g_fail);
    if (g_fail == 0) std::printf("ALL TESTS PASSED\n");
    return g_fail ? 1 : 0;
}
