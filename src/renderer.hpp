#pragma once
#include "gl_util.hpp"
#include <vector>

struct PointLight { glm::vec3 pos; float radius; glm::vec3 color; };

// Must match `layout(std140) uniform Frame` in shaders/common.glsl.
struct FrameUBO {
    glm::mat4 view, proj, viewProj, lightVP;
    glm::vec4 camPos;      // xyz, w = time
    glm::vec4 moonDir;     // xyz toward light, w = flash
    glm::vec4 moonColor;
    glm::vec4 ambient;     // rgb, a = fog density
    glm::vec4 fogColor;    // rgb, a = wetness
    glm::vec4 lightPos[16];
    glm::vec4 lightCol[16];
    glm::vec4 counts;      // x = lights, y = shadows on, z = fire intensity
};

class Renderer {
public:
    void init(int w, int h);
    void resize(int w, int h);        // w or h == 0 (minimised) is a no-op
    int width() const { return w_; }
    int height() const { return h_; }

    void uploadFrame(const FrameUBO& f);

    // Shadow map pass: bind, draw depth-only, end.
    void beginShadow();
    void endShadow();
    GLuint shadowTex() const { return shadowTex_; }
    static constexpr int kShadowRes = 2048;

    void beginScene();                 // binds the multisampled HDR target and clears it
    void endScene(float exposure, float bloomStrength, float flash, float time, GLuint targetFbo = 0);

private:
    void makeTargets();
    void destroyTargets();
    int w_ = 0, h_ = 0;
    GLuint ubo_ = 0;
    GLuint msFbo_ = 0, msColor_ = 0, msDepth_ = 0;
    GLuint resFbo_ = 0, resColor_ = 0;
    GLuint shadowFbo_ = 0, shadowTex_ = 0;
    static constexpr int kLevels = 6;
    GLuint bloomFbo_[kLevels]{}, bloomTex_[kLevels]{};
    int bw_[kLevels]{}, bh_[kLevels]{};
    GLuint emptyVao_ = 0;
    Shader down_, up_, tonemap_;
    GLint prevViewport_[4]{};
};
