#pragma once
// Procedural galleon geometry and wave-following pose (GL-free).
// Ship-local frame: +Z = bow, +Y = up, +X = starboard, waterline at y = 0.
#include "geometry.hpp"
#include <glm/glm.hpp>
#include <vector>

struct GalleonParams {
    float length = 34.f;
    float beam = 9.f;
    float draft = 3.2f;
    glm::vec3 hullColor{0.10f, 0.075f, 0.06f};
    glm::vec3 trimColor{0.55f, 0.38f, 0.12f};
    glm::vec3 sailColor{0.78f, 0.72f, 0.60f};
    int seed = 1;
};

// Hull profile functions, shared by the mesh builder and the deck clamp.
struct HullProfile {
    float L, B, draft;
    float halfBeam(float z) const;     // max half-width of the hull at z
    float railHeight(float z) const;   // top of the bulwark
    float deckHeight(float z) const;   // walkable deck surface
    float keelDepth(float z) const;    // bottom (negative)
    float sectionWidth(float t) const; // 0..1 width factor from keel (t=0) to rail (t=1)
};

struct DeckOutline {
    HullProfile hull;
    float zMin, zMax;
    float halfBeam(float z) const;     // walkable half-width (inside bulwark)
    float deckHeight(float z) const { return hull.deckHeight(z); }
};

struct GalleonGeometry {
    MeshData hull, deck, wood, sails, mainSails, rigging;  // rigging is GL_LINES
    std::vector<glm::vec3> lanternLocal;
    glm::vec3 mainMastTop;
    glm::vec3 mainMastBase;
    DeckOutline outline;
    HullProfile profile;
};

GalleonGeometry buildGalleonGeometry(const GalleonParams& p);

struct ShipState {
    glm::vec2 xz{0.f};
    float heading = 0.f;     // radians, 0 = sailing toward +Z
    float speed = 3.f;       // m/s
    float sinkProgress = 0.f;
};

glm::vec3 shipForward(float heading);
// World transform of a ship riding the waves at time t.
glm::mat4 shipPose(const ShipState& s, float t, float length, float beam);
