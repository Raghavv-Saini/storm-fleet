#include "ship.hpp"

GalleonMeshes uploadGalleon(const GalleonGeometry& g, const GalleonParams& p) {
    GalleonMeshes m;
    m.hull = Mesh::create(g.hull);
    m.deck = Mesh::create(g.deck);
    m.wood = Mesh::create(g.wood);
    m.sails = Mesh::create(g.sails);
    m.mainSails = Mesh::create(g.mainSails);
    m.rigging = Mesh::create(g.rigging, GL_LINES);
    m.lanternLocal = g.lanternLocal;
    m.mainMastTop = g.mainMastTop;
    m.mainMastBase = g.mainMastBase;
    m.outline = g.outline;
    m.params = p;
    return m;
}

void drawGalleon(const Shader& lit, const GalleonMeshes& m, const glm::mat4& model, const DrawOpts& o) {
    lit.set("uModel", model);
    lit.set("uShipSeed", o.seed);
    lit.set("uChar", o.charAmount);
    lit.set("uShadowPass", o.shadowPass);
    lit.set("uBillow", 0.f);
    glDisable(GL_CULL_FACE);  // hulls are lofted open shells viewed from both sides
    m.hull.draw();
    m.deck.draw();
    m.wood.draw();
    if (o.shadowPass == 0) m.rigging.draw();
    lit.set("uBillow", o.billow);
    m.sails.draw();
    if (o.showMainSails) m.mainSails.draw();
}
