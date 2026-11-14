#pragma once
// GL-free vertex/mesh data shared by procedural builders and the renderer.
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

// Material ids (stored per-vertex, read by lit.frag)
enum Mat : int { MAT_WOOD = 0, MAT_CANVAS = 1, MAT_PLAIN = 2, MAT_EMISSIVE = 3, MAT_DECK = 4, MAT_ROPE = 5 };

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 color;
    float mat;
};

struct MeshData {
    std::vector<Vertex> verts;
    std::vector<uint32_t> idx;

    uint32_t add(const Vertex& v) { verts.push_back(v); return (uint32_t)verts.size() - 1; }
    void tri(uint32_t a, uint32_t b, uint32_t c) { idx.push_back(a); idx.push_back(b); idx.push_back(c); }
    void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { tri(a, b, c); tri(a, c, d); }
    void append(const MeshData& o);
    void recomputeNormals();  // smooth normals from triangles
};
