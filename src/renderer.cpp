#include "renderer.hpp"
#include <algorithm>
#include <cstdio>

void Renderer::init(int w, int h) {
    glGenBuffers(1, &ubo_);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(FrameUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_);

    glGenVertexArrays(1, &emptyVao_);
    down_ = Shader::load("post.vert", "bloom_down.frag");
    up_ = Shader::load("post.vert", "bloom_up.frag");
    tonemap_ = Shader::load("post.vert", "tonemap.frag");

    // shadow map
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowRes, kShadowRes, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glGenFramebuffers(1, &shadowFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    resize(w, h);
}

void Renderer::destroyTargets() {
    if (msFbo_) { glDeleteFramebuffers(1, &msFbo_); glDeleteRenderbuffers(1, &msColor_); glDeleteRenderbuffers(1, &msDepth_); }
    if (resFbo_) { glDeleteFramebuffers(1, &resFbo_); glDeleteTextures(1, &resColor_); }
    for (int i = 0; i < kLevels; ++i)
        if (bloomFbo_[i]) { glDeleteFramebuffers(1, &bloomFbo_[i]); glDeleteTextures(1, &bloomTex_[i]); }
    msFbo_ = resFbo_ = 0;
    std::fill(bloomFbo_, bloomFbo_ + kLevels, 0);
}

static GLuint makeHdrTex(int w, int h) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

void Renderer::makeTargets() {
    const int samples = 4;
    glGenFramebuffers(1, &msFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glGenRenderbuffers(1, &msColor_);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, w_, h_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor_);
    glGenRenderbuffers(1, &msDepth_);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth_);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, w_, h_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, msDepth_);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) std::fprintf(stderr, "MS framebuffer incomplete\n");

    resColor_ = makeHdrTex(w_, h_);
    glGenFramebuffers(1, &resFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, resFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resColor_, 0);

    int bw = std::max(1, w_ / 2), bh = std::max(1, h_ / 2);
    for (int i = 0; i < kLevels; ++i) {
        bw_[i] = bw; bh_[i] = bh;
        bloomTex_[i] = makeHdrTex(bw, bh);
        glGenFramebuffers(1, &bloomFbo_[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex_[i], 0);
        bw = std::max(1, bw / 2); bh = std::max(1, bh / 2);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::resize(int w, int h) {
    if (w <= 0 || h <= 0) return;      // minimised: keep old targets
    if (w == w_ && h == h_ && msFbo_) return;
    w_ = w; h_ = h;
    destroyTargets();
    makeTargets();
}

void Renderer::uploadFrame(const FrameUBO& f) {
    glBindBuffer(GL_UNIFORM_BUFFER, ubo_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(FrameUBO), &f);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo_);
}

void Renderer::beginShadow() {
    glGetIntegerv(GL_VIEWPORT, prevViewport_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, kShadowRes, kShadowRes);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.f, 4.f);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
}
void Renderer::endShadow() {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(prevViewport_[0], prevViewport_[1], prevViewport_[2], prevViewport_[3]);
}

void Renderer::beginScene() {
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo_);
    glViewport(0, 0, w_, h_);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_MULTISAMPLE);
}

void Renderer::endScene(float exposure, float bloomStrength, float flash, float time, GLuint targetFbo) {
    // resolve MSAA
    glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resFbo_);
    glBlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glBindVertexArray(emptyVao_);

    // downsample chain (first level does the bright pass)
    down_.use();
    for (int i = 0; i < kLevels; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[i]);
        glViewport(0, 0, bw_[i], bh_[i]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, i == 0 ? resColor_ : bloomTex_[i - 1]);
        int sw = i == 0 ? w_ : bw_[i - 1], sh = i == 0 ? h_ : bh_[i - 1];
        down_.set("uSrc", 0);
        down_.set("uTexel", glm::vec2(1.f / sw, 1.f / sh));
        down_.set("uFirst", i == 0 ? 1 : 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    // upsample, additively accumulating onto each larger level
    up_.use();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    for (int i = kLevels - 1; i > 0; --i) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[i - 1]);
        glViewport(0, 0, bw_[i - 1], bh_[i - 1]);
        glBindTexture(GL_TEXTURE_2D, bloomTex_[i]);
        up_.set("uSrc", 0);
        up_.set("uTexel", glm::vec2(1.f / bw_[i], 1.f / bh_[i]));
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);

    // tonemap to the target
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(0, 0, w_, h_);
    tonemap_.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, resColor_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTex_[0]);
    tonemap_.set("uScene", 0);
    tonemap_.set("uBloom", 1);
    tonemap_.set("uExposure", exposure);
    tonemap_.set("uBloomStrength", bloomStrength);
    tonemap_.set("uFlash", flash);
    tonemap_.set("uRes", glm::vec2((float)w_, (float)h_));
    tonemap_.set("uTime", time);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
}
