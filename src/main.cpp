// Storm Fleet — procedural galleons in a North Sea storm. Modern OpenGL 3.3 core.
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "audio.hpp"
#include "camera.hpp"
#include "event.hpp"
#include "gl_util.hpp"
#include "ocean.hpp"
#include "particles.hpp"
#include "renderer.hpp"
#include "ship.hpp"
#include "sky.hpp"
#include "textures.hpp"
#include "weather.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Prefer the discrete GPU on hybrid-graphics laptops.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace {
constexpr int kStruck = 3;           // fleet index of the ship the lightning hits
constexpr float kFleetSpeed = 2.5f;  // m/s

struct Args {
    std::string shot;
    int scene = 1;
    float time = 0.f;
    bool hasCam = false;
    glm::vec3 camPos{0.f};
    float camYaw = 0.f, camPitch = 0.f;
    bool hasDeck = false;
    glm::vec2 deckPos{0.f};
    float deckYaw = 0.f, deckPitch = 0.f;
    bool noAudio = false;
    float bench = 0.f;   // >0: run the live loop for N seconds, print fps, exit
    int w = 1600, h = 900;
    float exposure = 1.7f;
};

struct FleetShip {
    GalleonMeshes mesh;
    ShipState st;
    glm::vec2 offset;
    glm::mat4 model{1.f};
};

struct Game {
    Args args;
    GLFWwindow* win = nullptr;
    Renderer renderer;
    Ocean ocean;
    Sky sky;
    Weather weather;
    Particles particles;
    StrikeEvent event;
    Shader oceanSh, litSh, particleSh, boltSh, rainSh;
    GLuint woodTex = 0, canvasTex = 0, detailTex = 0;
    std::vector<FleetShip> fleet;

    FlyCamera fly;
    DeckCamera deck;
    int scene = 1;

    float t = 0.f;
    glm::vec2 fleetCenter{0.f};
    float struckLag = 0.f;
    bool struckDone = false;
    int strikeCount = 0;

    // input
    InputState in;
    double lastMx = 0, lastMy = 0;
    bool firstMouse = true;
    bool showHelp = true;

    void initWorld();
    void step(float dt);
    void emitFire(float dt);
    void gatherLights(glm::vec3 camPos, std::vector<PointLight>& out);
    void render(const glm::mat4& view, glm::vec3 camPos, GLuint targetFbo);
    void retrigger();
};

Game* G = nullptr;

// ---- Fleet definition: positions are relative to the moving fleet centre ----
struct ShipDef { glm::vec2 offset; float length, beam; glm::vec3 hull, trim, sail; float headingBias; };
const ShipDef kDefs[6] = {
    {{0.f, 0.f},      34.f, 9.0f, {0.10f, 0.075f, 0.06f}, {0.55f, 0.38f, 0.12f}, {0.78f, 0.72f, 0.60f}, 0.00f},
    {{-62.f, 40.f},   30.f, 8.0f, {0.07f, 0.08f, 0.10f},  {0.60f, 0.45f, 0.15f}, {0.72f, 0.70f, 0.64f}, 0.10f},
    {{58.f, 78.f},    38.f, 9.8f, {0.12f, 0.05f, 0.04f},  {0.50f, 0.34f, 0.10f}, {0.80f, 0.74f, 0.62f}, -0.08f},
    {{-22.f, 118.f},  36.f, 9.4f, {0.09f, 0.07f, 0.05f},  {0.58f, 0.40f, 0.12f}, {0.82f, 0.76f, 0.64f}, 0.05f},
    {{84.f, 8.f},     28.f, 7.6f, {0.06f, 0.07f, 0.06f},  {0.45f, 0.36f, 0.14f}, {0.70f, 0.68f, 0.60f}, -0.14f},
    {{-96.f, -34.f},  32.f, 8.6f, {0.11f, 0.09f, 0.07f},  {0.52f, 0.30f, 0.10f}, {0.76f, 0.72f, 0.62f}, 0.12f},
};

void Game::initWorld() {
    renderer.init(args.w, args.h);
    ocean.init();
    sky.init();
    weather.init();
    particles.init();
    oceanSh = Shader::load("ocean.vert", "ocean.frag");
    litSh = Shader::load("lit.vert", "lit.frag");
    particleSh = Shader::load("particle.vert", "particle.frag");
    boltSh = Shader::load("bolt.vert", "bolt.frag");
    rainSh = Shader::load("rain.vert", "rain.frag");
    woodTex = makeWoodTexture(3);
    canvasTex = makeCanvasTexture();
    detailTex = makeWaterDetailTexture();

    for (int i = 0; i < 6; ++i) {
        const ShipDef& d = kDefs[i];
        GalleonParams p;
        p.length = d.length; p.beam = d.beam; p.hullColor = d.hull; p.trimColor = d.trim; p.sailColor = d.sail;
        p.seed = i + 1;
        FleetShip s;
        s.mesh = uploadGalleon(buildGalleonGeometry(p), p);
        s.offset = d.offset;
        s.st.heading = d.headingBias;
        s.st.speed = kFleetSpeed;
        fleet.push_back(std::move(s));
    }
    fly.pos = {-70.f, 20.f, -85.f};
    fly.yaw = 0.22f;
    fly.pitch = -0.04f;
    if (args.hasCam) { fly.pos = args.camPos; fly.yaw = args.camYaw; fly.pitch = args.camPitch; }
    if (args.hasDeck) {
        deck.local = args.deckPos; deck.yaw = args.deckYaw; deck.pitch = args.deckPitch;
        const DeckOutline& o = fleet[0].mesh.outline;
        deck.local = clampToDeck(resolveObstacles(clampToDeck(deck.local, o), o), o);
    }
    scene = args.scene;
    // positions/poses for t = 0
    step(0.f);
}

void Game::retrigger() {
    event.retrigger(t, 3.f);
    struckDone = false;
    struckLag = 0.f;
    particles.clear();
    fleet[kStruck].st.sinkProgress = 0.f;
}

void Game::emitFire(float dt) {
    float I = event.fireIntensity(t);
    if (I < 0.01f) return;
    FleetShip& sh = fleet[kStruck];
    const GalleonMeshes& m = sh.mesh;
    float L = m.params.length;
    float burnT = event.burnTime(t);
    float climb = glm::smoothstep(0.f, 18.f, burnT);
    glm::vec3 shipVel = shipForward(sh.st.heading) * sh.st.speed;
    float mastZ[3] = {0.30f * L, -0.02f * L, -0.33f * L};
    float mastH[3] = {25.f, 30.f, 19.f};
    auto localPoint = [&](glm::vec3 spread, float heightMax) {
        float r = particles.rnd();
        int mi = r < 0.55f ? 1 : (r < 0.8f ? 0 : 2);
        float z = mastZ[mi];
        float base = m.outline.deckHeight(z);
        float y = base + 1.f + particles.rnd() * (4.f + (mastH[mi] * heightMax) * climb);
        return glm::vec3((particles.rnd() - 0.5f) * spread.x, y, z + (particles.rnd() - 0.5f) * spread.z);
    };
    auto world = [&](glm::vec3 l) { return glm::vec3(sh.model * glm::vec4(l, 1.f)); };

    auto count = [&](float rate) { float x = rate * dt * I; int n = (int)x; if (particles.rnd() < x - n) ++n; return n; };
    for (int i = 0; i < count(260.f); ++i) {
        glm::vec3 lp;
        float r = particles.rnd();
        if (r < 0.5f) lp = localPoint({1.2f, 1.2f, 0}, 0.85f);                  // burning masts
        else if (r < 0.8f) lp = localPoint({6.f, 1.5f, 0}, 0.6f);               // sails
        else { float z = mastZ[1] + (particles.rnd() - 0.5f) * 16.f; lp = glm::vec3((particles.rnd() - 0.5f) * 5.f, m.outline.deckHeight(z) + 0.4f, z); }
        glm::vec3 v = shipVel * 0.5f + glm::vec3((particles.rnd() - 0.5f) * 1.4f, 2.5f + particles.rnd() * 3.5f, (particles.rnd() - 0.5f) * 1.4f);
        particles.spawn(PKind::Fire, world(lp), v, 0.7f + particles.rnd() * 0.8f, 1.3f + particles.rnd() * 1.4f);
    }
    for (int i = 0; i < count(70.f); ++i) {
        glm::vec3 lp = localPoint({4.f, 3.f, 0}, 1.2f);
        glm::vec3 v = shipVel * 0.3f + glm::vec3((particles.rnd() - 0.5f) * 1.5f, 2.5f + particles.rnd() * 2.f, (particles.rnd() - 0.5f) * 1.5f);
        particles.spawn(PKind::Smoke, world(lp) + glm::vec3(0, 3.f, 0), v, 4.5f + particles.rnd() * 3.f, 2.2f + particles.rnd() * 1.8f);
    }
    for (int i = 0; i < count(35.f); ++i) {
        glm::vec3 lp = localPoint({3.f, 3.f, 0}, 1.0f);
        glm::vec3 v = glm::vec3((particles.rnd() - 0.5f) * 5.f, 2.f + particles.rnd() * 5.f, (particles.rnd() - 0.5f) * 5.f);
        particles.spawn(PKind::Ember, world(lp), v, 1.5f + particles.rnd() * 2.f, 0.09f + particles.rnd() * 0.08f);
    }
}

void Game::step(float dt) {
    t += dt;
    fleetCenter.y += kFleetSpeed * dt;

    // The struck ship loses way after the strike and falls behind the fleet.
    float burnT = event.burnTime(t);
    float shipSpeed = kFleetSpeed * (1.f - glm::smoothstep(0.f, 50.f, burnT));
    if (t >= event.t0) struckLag += (kFleetSpeed - shipSpeed) * dt;

    for (int i = 0; i < (int)fleet.size(); ++i) {
        FleetShip& s = fleet[i];
        s.st.xz = fleetCenter + s.offset;
        if (i == kStruck) {
            s.st.xz.y -= struckLag;
            s.st.sinkProgress = event.sinkProgress(t);
            s.st.speed = shipSpeed;
        }
        s.model = shipPose(s.st, t, s.mesh.params.length, s.mesh.params.beam);
    }

    weather.update(dt, t, glm::vec3(fleetCenter.x, 0.f, fleetCenter.y));

    if (!struckDone && t >= event.t0) {
        struckDone = true;
        ++strikeCount;
        FleetShip& s = fleet[kStruck];
        glm::vec3 top = glm::vec3(s.model * glm::vec4(s.mesh.mainMastTop, 1.f));
        weather.triggerStrike(top, 7u + strikeCount * 13u);
    }
    for (const ThunderEvent& th : weather.takeThunder()) audio::thunder(th.distance, th.delay + th.distance / 220.f);

    emitFire(dt);
    particles.update(dt, glm::vec3(9.f, 0.f, -2.f) * 0.5f);
}

void Game::gatherLights(glm::vec3 camPos, std::vector<PointLight>& out) {
    out.clear();
    float I = event.fireIntensity(t);
    if (I > 0.02f) {
        FleetShip& s = fleet[kStruck];
        float L = s.mesh.params.length;
        float flick = 0.8f + 0.2f * std::sin(t * 23.f) * std::sin(t * 13.7f + 1.f) + 0.1f * std::sin(t * 41.f);
        glm::vec3 col = glm::vec3(6.f, 2.2f, 0.5f) * I * flick;
        float mz = -0.02f * L;
        float dy = s.mesh.outline.deckHeight(mz);
        glm::vec3 pts[3] = {{0.f, dy + 5.f, mz}, {0.f, dy + 12.f * glm::smoothstep(0.f, 18.f, event.burnTime(t)) + 3.f, mz + 0.3f * L},
                            {0.f, dy + 3.f, mz - 6.f}};
        for (auto& p : pts) out.push_back({glm::vec3(s.model * glm::vec4(p, 1.f)), 80.f, col});
    }
    struct Cand { float d; PointLight l; };
    std::vector<Cand> cands;
    for (int i = 0; i < (int)fleet.size(); ++i) {
        if (i == kStruck && fleet[i].st.sinkProgress > 0.8f) continue;
        for (const glm::vec3& lp : fleet[i].mesh.lanternLocal) {
            glm::vec3 w = glm::vec3(fleet[i].model * glm::vec4(lp, 1.f));
            float flick = 0.9f + 0.1f * std::sin(t * 9.f + w.x);
            cands.push_back({glm::length2(w - camPos), {w, 20.f, glm::vec3(1.2f, 0.54f, 0.17f) * flick}});
        }
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.d < b.d; });
    for (auto& c : cands) {
        if (out.size() >= 16) break;
        out.push_back(c.l);
    }
}

void Game::render(const glm::mat4& view, glm::vec3 camPos, GLuint targetFbo) {
    float aspect = (float)renderer.width() / (float)std::max(1, renderer.height());
    glm::mat4 proj = glm::perspective(glm::radians(scene == 2 ? 62.f : 70.f), aspect, 0.25f, 7000.f);

    FrameUBO f{};
    f.view = view; f.proj = proj; f.viewProj = proj * view;
    float flash = weather.flash();

    glm::vec3 moonBase = glm::normalize(glm::vec3(-0.35f, 0.5f, 0.8f));
    glm::vec3 L = glm::normalize(glm::mix(moonBase, weather.flashDir(camPos), glm::clamp(flash * 2.f, 0.f, 1.f)));

    // moon shadow map fitted around the camera
    float half = scene == 1 ? 55.f : 110.f;
    glm::mat4 lightView = glm::lookAt(camPos + L * 160.f, camPos, glm::abs(L.y) > 0.95f ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0));
    glm::mat4 lightProj = glm::ortho(-half, half, -half, half, 1.f, 330.f);
    f.lightVP = lightProj * lightView;

    f.camPos = glm::vec4(camPos, wrapTime(t));
    f.moonDir = glm::vec4(L, flash);
    f.moonColor = glm::vec4(glm::vec3(0.50f, 0.60f, 0.90f) * (0.9f + 8.f * flash), 1.f);
    f.ambient = glm::vec4(glm::vec3(0.020f, 0.028f, 0.042f) * (1.f + 5.f * flash), 0.0042f);
    f.fogColor = glm::vec4(0.040f, 0.052f, 0.070f, 0.75f);
    std::vector<PointLight> lights;
    gatherLights(camPos, lights);
    for (size_t i = 0; i < lights.size() && i < 16; ++i) {
        f.lightPos[i] = glm::vec4(lights[i].pos, lights[i].radius);
        f.lightCol[i] = glm::vec4(lights[i].color, 1.f);
    }
    f.counts = glm::vec4((float)std::min<size_t>(lights.size(), 16), 1.f, event.fireIntensity(t), 0.f);
    renderer.uploadFrame(f);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, woodTex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, canvasTex);
    glActiveTexture(GL_TEXTURE0);

    auto drawShips = [&](int shadowPass) {
        litSh.use();
        litSh.set("uWood", 0); litSh.set("uCanvas", 1); litSh.set("uShadowMap", 2);
        litSh.set("uShadowVP", f.lightVP);
        litSh.set("uWet", 0.f);
        for (int i = 0; i < (int)fleet.size(); ++i) {
            FleetShip& s = fleet[i];
            if (i == kStruck && s.st.sinkProgress >= 0.999f) continue;
            DrawOpts o;
            o.seed = i * 3.7f;
            o.shadowPass = shadowPass;
            if (i == kStruck) {
                float b = event.burnTime(t);
                o.charAmount = glm::smoothstep(0.f, 40.f, b);
                o.showMainSails = b < 26.f;
            }
            drawGalleon(litSh, s.mesh, s.model, o);
        }
    };

    // ---- shadow pass ----
    renderer.beginShadow();
    drawShips(1);
    renderer.endShadow();

    // ---- scene ----
    renderer.beginScene();
    sky.draw(glm::inverse(f.viewProj));

    glDisable(GL_CULL_FACE);
    ocean.draw(oceanSh, camPos, detailTex);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, woodTex);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, canvasTex);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, renderer.shadowTex());
    glActiveTexture(GL_TEXTURE0);
    drawShips(0);

    weather.drawBolts(boltSh);
    particles.draw(particleSh, camPos);
    weather.drawRain(rainSh, camPos, 1.f);

    float exposure = args.exposure;
    renderer.endScene(exposure, 0.55f, flash, t, targetFbo);
}

// ---------------- input ----------------
void keyCb(GLFWwindow* w, int key, int, int action, int) {
    bool down = action != GLFW_RELEASE;
    switch (key) {
        case GLFW_KEY_W: G->in.w = down; break;
        case GLFW_KEY_A: G->in.a = down; break;
        case GLFW_KEY_S: G->in.s = down; break;
        case GLFW_KEY_D: G->in.d = down; break;
        case GLFW_KEY_SPACE: G->in.up = down; break;
        case GLFW_KEY_LEFT_CONTROL: case GLFW_KEY_C: G->in.down = down; break;
        case GLFW_KEY_LEFT_SHIFT: G->in.fast = down; break;
        default: break;
    }
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
    if (key == GLFW_KEY_1) G->scene = 1;
    if (key == GLFW_KEY_2) G->scene = 2;
    if (key == GLFW_KEY_R) G->retrigger();
    if (key == GLFW_KEY_L) G->weather.triggerStrike(glm::vec3(G->fleetCenter.x + 150.f, 0.f, G->fleetCenter.y + 300.f), 99u + (uint32_t)(G->t * 10));
    if (key == GLFW_KEY_M) { static bool m = false; m = !m; audio::setMuted(m); }
    if (key == GLFW_KEY_F1) G->showHelp = !G->showHelp;
}
void mouseCb(GLFWwindow*, double x, double y) {
    if (G->firstMouse) { G->lastMx = x; G->lastMy = y; G->firstMouse = false; }
    G->in.mouseDx += (float)(x - G->lastMx);
    G->in.mouseDy += (float)(y - G->lastMy);
    G->lastMx = x; G->lastMy = y;
}
void resizeCb(GLFWwindow*, int w, int h) { G->renderer.resize(w, h); }

bool parseArgs(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        auto f = [&](int k) { return (float)std::atof(argv[i + k]); };
        if (s == "--shot" && i + 1 < argc) a.shot = argv[++i];
        else if (s == "--scene" && i + 1 < argc) a.scene = std::atoi(argv[++i]);
        else if (s == "--time" && i + 1 < argc) a.time = (float)std::atof(argv[++i]);
        else if (s == "--cam" && i + 5 < argc) { a.hasCam = true; a.camPos = {f(1), f(2), f(3)}; a.camYaw = f(4); a.camPitch = f(5); i += 5; }
        else if (s == "--deck" && i + 4 < argc) { a.hasDeck = true; a.deckPos = {f(1), f(2)}; a.deckYaw = f(3); a.deckPitch = f(4); i += 4; }
        else if (s == "--size" && i + 2 < argc) { a.w = std::atoi(argv[i + 1]); a.h = std::atoi(argv[i + 2]); i += 2; }
        else if (s == "--exposure" && i + 1 < argc) a.exposure = (float)std::atof(argv[++i]);
        else if (s == "--noaudio") a.noAudio = true;
        else if (s == "--bench" && i + 1 < argc) a.bench = (float)std::atof(argv[++i]);
        else { std::fprintf(stderr, "unknown or incomplete argument: %s\n", s.c_str()); return false; }
    }
    return true;
}

glm::mat4 currentView(Game& g, glm::vec3& camPos) {
    if (g.scene == 1) {
        FleetShip& fs = g.fleet[0];
        const DeckOutline& o = fs.mesh.outline;
        camPos = glm::vec3(fs.model * glm::vec4(g.deck.eyeLocal(o), 1.f));
        return g.deck.view(fs.model, o);
    }
    camPos = g.fly.pos;
    return g.fly.view();
}

int runShot(Game& g) {
    // Fast-forward the simulation, then render one frame to an offscreen target and save it.
    const float dt = 0.05f;
    float target = g.args.time;
    while (g.t + dt < target) g.step(dt);
    if (target > g.t) g.step(target - g.t);

    GLuint tex, fbo;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g.args.w, g.args.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    glm::vec3 camPos;
    glm::mat4 view = currentView(g, camPos);
    g.render(view, camPos, fbo);
    checkGL("shot");

    std::vector<unsigned char> px((size_t)g.args.w * g.args.h * 4), flipped(px.size());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadPixels(0, 0, g.args.w, g.args.h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < g.args.h; ++y)
        std::memcpy(&flipped[(size_t)y * g.args.w * 4], &px[(size_t)(g.args.h - 1 - y) * g.args.w * 4], (size_t)g.args.w * 4);
    if (!stbi_write_png(g.args.shot.c_str(), g.args.w, g.args.h, 4, flipped.data(), g.args.w * 4)) {
        std::fprintf(stderr, "failed to write %s\n", g.args.shot.c_str());
        return 1;
    }
    std::printf("wrote %s (t=%.2f, particles=%zu, flash=%.2f)\n", g.args.shot.c_str(), g.t, g.particles.count(), g.weather.flash());
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    Args args;
    if (!parseArgs(argc, argv, args)) return 2;
    if (!glfwInit()) { std::fprintf(stderr, "glfwInit failed\n"); return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (!args.shot.empty()) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* win = glfwCreateWindow(args.w, args.h, "Storm Fleet", nullptr, nullptr);
    if (!win) { std::fprintf(stderr, "window creation failed (need OpenGL 3.3)\n"); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(args.shot.empty() ? 1 : 0);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::fprintf(stderr, "glad failed\n"); return 1; }
    std::printf("GL: %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    Game game;
    G = &game;
    game.args = args;
    game.win = win;
    int rc = 0;
    try {
        game.initWorld();
        if (!args.shot.empty()) {
            rc = runShot(game);
        } else {
            if (!args.noAudio) audio::init();
            glfwSetKeyCallback(win, keyCb);
            glfwSetCursorPosCallback(win, mouseCb);
            glfwSetFramebufferSizeCallback(win, resizeCb);
            glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            int fw, fh;
            glfwGetFramebufferSize(win, &fw, &fh);
            game.renderer.resize(fw, fh);

            double last = glfwGetTime(), titleTimer = 0.0, startT = last, worst = 0.0;
            int frames = 0, totalFrames = 0;
            while (!glfwWindowShouldClose(win)) {
                double now = glfwGetTime();
                float dt = (float)std::min(now - last, 0.1);
                if (totalFrames > 5) worst = std::max(worst, now - last);
                last = now;
                if (args.bench > 0.f) {
                    // scripted flight: sweep the camera so the benchmark exercises the whole scene
                    game.in.w = true;
                    game.in.mouseDx = 2.0f * std::sin((float)(now - startT) * 0.6f);
                    if (now - startT > args.bench) { std::printf("bench: %d frames in %.1fs = %.1f fps avg, worst frame %.1f ms\n", totalFrames, now - startT, totalFrames / (now - startT), worst * 1000.0); break; }
                }
                glfwPollEvents();

                // fixed small steps keep particle and wave logic stable at any frame rate
                float rem = dt;
                while (rem > 0.f) { float s = std::min(rem, 1.f / 30.f); game.step(s); rem -= s; }

                if (game.scene == 1) game.deck.update(game.in, dt, game.fleet[0].mesh.outline);
                else game.fly.update(game.in, dt, game.t);
                game.in.mouseDx = game.in.mouseDy = 0.f;

                glm::vec3 camPos;
                glm::mat4 view = currentView(game, camPos);
                game.render(view, camPos, 0);
                if (args.bench > 0.f) glFinish();   // measure real GPU time, not just queued commands
                glfwSwapBuffers(win);

                ++frames; ++totalFrames;
                titleTimer += dt;
                if (titleTimer >= 0.5) {
                    char title[256];
                    const char* ph[] = {"calm", "STRIKE", "burning", "sinking", "sunk"};
                    std::snprintf(title, sizeof title, "Storm Fleet | %s scene | %.0f fps | ship #4: %s%s", game.scene == 1 ? "Deck" : "Flying",
                                  frames / titleTimer, ph[(int)game.event.phase(game.t)],
                                  game.showHelp ? " | 1/2 scene, WASD+mouse, Shift fast, Space/C up-down, R replay strike, L lightning, M mute, F1 hide help, Esc quit" : "");
                    glfwSetWindowTitle(win, title);
                    frames = 0; titleTimer = 0.0;
                }
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        rc = 1;
    }
    audio::shutdown();
    glfwTerminate();
    return rc;
}
