#pragma once
#include "gl_util.hpp"
#include "ship_core.hpp"

struct GalleonMeshes {
    Mesh hull, deck, wood, sails, mainSails, rigging;
    std::vector<glm::vec3> lanternLocal;
    glm::vec3 mainMastTop, mainMastBase;
    DeckOutline outline;
    GalleonParams params;
};

GalleonMeshes uploadGalleon(const GalleonGeometry& g, const GalleonParams& p);

struct DrawOpts {
    float billow = 1.f;
    float seed = 0.f;
    float charAmount = 0.f;     // 0..1 burn char
    bool showMainSails = true;
    int shadowPass = 0;
};

// Draws every part of the ship with the `lit` shader (textures bound by the caller to units 0..2).
void drawGalleon(const Shader& lit, const GalleonMeshes& m, const glm::mat4& model, const DrawOpts& o);
