# Storm Fleet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One modern OpenGL program that shows a procedurally built galleon fleet in a night storm. It has two explorable scenes (walking the deck and free flight), plus a lightning strike → fire → sinking event.

**Architecture:** A C++17 forward renderer that draws into an HDR framebuffer, then runs bloom and ACES tone mapping. All geometry and textures are generated in code at startup. Wave math lives in one CPU function, and the identical formula runs in GLSL: the CPU passes the parameters as uniforms, so ships float exactly on the rendered water. Pure CPU logic (waves, ship pose, deck clamp, lightning generation, event timeline) is unit-tested. Rendering is verified by a `--shot` flag that renders a fixed time and camera to a PNG, which is then inspected.

**Tech Stack:** WinLibs GCC 16 (UCRT, 64-bit), CMake 4, GLFW 3.4, GLAD (GL 3.3 core), GLM 1.0, stb_image_write, miniaudio (tier 3). Dependencies come from CMake FetchContent, except GLAD, which is vendored into `external/glad`.

**Spec:** `docs/design.md`

## Global Constraints

- OpenGL **3.3 core**, C++17, 64-bit build.
- **No external 3D models or image assets.** All geometry and textures are generated in code.
- Original ship designs only. No film-franchise names, flags or likenesses.
- Target ≥ 30 fps on the GTX 1650 Ti. Export `NvOptimusEnablement = 1` and `AmdPowerXpressRequestHighPerformance = 1` from `main.cpp`.
- Controls exactly as in the spec: `1`/`2` change scene, WASD + mouse, Space/Ctrl move up and down (flying only), Shift is fast, `R` retriggers the event, `L` is a manual flash, `Esc` quits.
- Fleet = 6 ships. Index 0 is the flagship (the deck scene). **Index 3 is the ship that gets struck.**
- The 5-hour budget is split into tiers. Tier 1 = Tasks 1–5, Tier 2 = Tasks 6–8, Tier 3 = Task 9. Stop at a tier boundary if time runs out, and always leave a building, running program.

## Review Focus

1. **Window resize / minimise to 0×0.** The framebuffers must be recreated on resize and skipped at 0 size, with no crash or GL error. Covered by Task 1: `resize(0,0)` is a no-op test.
2. **The deck camera at a ship's edge while the ship rolls.** It must never fall through or leave the deck. Covered by Task 3: `clampToDeck` tests.
3. **Switching scenes mid-event.** The event state is global, so both scenes show the same phase. Covered by Task 8: the timeline is independent of the scene.
4. **Long run times (t > 1 hour).** The waves must not lose float precision, so wrap the time sent to the shaders at a common period. Covered by Task 2: the `t = 4000` test.
5. **Pressing `R` while sinking.** Retriggering resets the struck ship to afloat with no leftover particles. Covered by Task 8: the reset test.

---

## File Structure

```
CMakeLists.txt
external/glad/            vendored GLAD (include/glad/glad.h, include/KHR/khrplatform.h, src/glad.c)
shaders/                  *.vert / *.frag, copied next to exe by CMake
src/main.cpp              window, loop, input, scene switch, --shot
src/gl_util.{hpp,cpp}     Shader, Mesh (VAO/VBO/EBO), Texture, Framebuffer helpers
src/camera.{hpp,cpp}      FlyCamera, DeckCamera, clampToDeck
src/ocean.{hpp,cpp}       Wave params, waveSample(), Ocean grid mesh + draw
src/ship.{hpp,cpp}        buildGalleon() mesh builder, ShipPose from waves
src/textures.{hpp,cpp}    procedural wood / canvas / noise textures
src/sky.{hpp,cpp}         sky dome + cloud shader
src/weather.{hpp,cpp}     lightning bolt generation + flash envelope, rain
src/particles.{hpp,cpp}   CPU particle system (fire, smoke, embers)
src/event.{hpp,cpp}       StrikeEvent timeline
src/renderer.{hpp,cpp}    HDR FBO, shadow map, bloom, tonemap, light uniforms
src/audio.{hpp,cpp}       synthesized thunder (tier 3)
tests/test_core.cpp       assert-based tests for pure CPU logic (no GL)
README.md
```

The core logic files (`ocean`, `ship`'s pose and builder, `camera` clamp, `weather` bolt generation, `event`) keep their GL-free functions in the same translation unit, behind a `CORE` section with no GL calls. They are compiled into a static library `core` that is linked by both `stormfleet` and `tests`.

---

### Task 1: Toolchain, skeleton, window, screenshot

**Files:** Create `CMakeLists.txt`, `external/glad/*`, `src/main.cpp`, `src/gl_util.{hpp,cpp}`, `tests/test_core.cpp`, `.gitignore`

**Interfaces:**
- Produces: `struct Shader { GLuint id; static Shader load(const std::string& vs, const std::string& fs); void use() const; void set(const char*, float/int/vec3/vec4/mat4) const; }`
- `struct Mesh { GLuint vao,vbo,ebo; GLsizei count; static Mesh create(const std::vector<Vertex>&, const std::vector<uint32_t>&); void draw() const; }` with `struct Vertex { glm::vec3 pos, normal; glm::vec2 uv; glm::vec3 color; }`
- `struct Framebuffer { GLuint fbo, color[2], depth; int w,h; void resize(int w,int h); }`. `resize(0,0)` is a no-op.
- `main` flags: `--shot <file.png> --scene <1|2> --time <sec>`. Renders one frame at a fixed time, writes a PNG, exits 0.

- [ ] Confirm `gcc --version` ≥ 13 and `cmake --version` from the new installs (open a fresh shell for PATH).
- [ ] Generate GLAD: `pip install glad` then `python -m glad --profile core --api gl=3.3 --generator c --out-path external/glad`. If Python is missing, download the 3.3-core zip from glad.dav1d.de.
- [ ] CMake: FetchContent glfw 3.4, glm 1.0.1, stb (for `stb_image_write.h`). Targets `core` (static), `stormfleet` (exe), `tests` (exe). Copy `shaders/` to the build directory after each build.
- [ ] Test `framebuffer_resize_zero_is_noop` can't run without GL, so it is checked manually: minimise the window and confirm there is no crash.
- [ ] `main.cpp`: open a 1600×900 window with a GL 3.3 core context and vsync, clear to a dark storm blue, and support `--shot`.
- [ ] Verify: `cmake -B build -G "MinGW Makefiles" && cmake --build build -j` succeeds. `build/stormfleet --shot shot.png` produces a blue PNG. `build/tests` prints `ALL TESTS PASSED`.
- [ ] `git init`, then commit "chore: project skeleton with GLFW/GLAD/GLM and screenshot mode".

### Task 2: Ocean (Gerstner)

**Files:** Create `src/ocean.{hpp,cpp}`, `shaders/ocean.{vert,frag}`, plus tests in `tests/test_core.cpp`

**Interfaces:**
- `struct Wave { glm::vec2 dir; float amplitude, wavelength, speed, steepness; };`
- `const std::array<Wave,6>& oceanWaves();`. The storm values are wavelengths 60, 37, 23, 14, 9, 5.5 m, amplitude ≈ wavelength/45, and directions clustered within ±40° of the wind (+X, slightly −Z).
- `struct WaveSample { glm::vec3 displaced; glm::vec3 normal; float height; };`
- `WaveSample waveSample(glm::vec2 xz, float t);`. `height` is the water height at world xz, found with 3 fixed-point iterations that undo the horizontal Gerstner displacement.
- `float wrapTime(float t);`, which returns `fmod(t, 3600.f)`.
- `class Ocean { void init(); void draw(const Shader&, glm::vec3 camPos); }`. The grid is 512×512 with 0.5 m spacing near the camera, plus a coarser outer ring out to 2 km. Both are snapped to the camera.

- [ ] Tests: `wave_height_bounded`: |height| ≤ the sum of the amplitudes for 1000 random points. `wave_normal_unit`: |normal| ≈ 1 ± 1e-3 and normal.y > 0.3. `wave_time_continuity`: height(t) and height(t+0.001) differ by < 0.05. `wave_large_time_ok`: t = 4000 gives a finite result.
- [ ] Run the tests → FAIL. Implement, then run → PASS.
- [ ] Shaders: the vertex shader uses the same Gerstner sum (the waves come in as a `uniform vec4 uWaveA[6], uWaveB[6]`). The fragment shader does Fresnel against the sky colour, a deep/shallow tint, foam from the Jacobian (the folding of the surface), and specular from the moon. Lightning and point lights are hooked up in Task 7.
- [ ] Verify: `--shot ocean.png --scene 2 --time 10` shows rolling waves. Inspect the image.
- [ ] Commit "feat: Gerstner ocean with CPU/GPU-matched wave sampling".

### Task 3: Cameras and scene switching

**Files:** Create `src/camera.{hpp,cpp}`, modify `src/main.cpp`, add tests

**Interfaces:**
- `struct FlyCamera { glm::vec3 pos; float yaw, pitch; glm::mat4 view() const; void update(const InputState&, float dt); }`. Speed is 15 m/s, or 60 with Shift.
- `struct DeckCamera { glm::vec2 local; float yaw, pitch; glm::mat4 view(const glm::mat4& shipModel) const; void update(const InputState&, float dt, const DeckOutline&); }`. Eye height is 1.7 m above the deck in ship-local space.
- `struct DeckOutline { float halfBeam(float z) const; float zMin, zMax; float deckHeight(float z) const; }`
- `glm::vec2 clampToDeck(glm::vec2 local, const DeckOutline&, float margin = 0.4f);`
- `struct InputState { bool w,a,s,d,up,down,fast; float mouseDx, mouseDy; }`

- [ ] Tests: `clamp_inside_unchanged`. `clamp_outside_beam_pulls_in`: (50, 0) → x == halfBeam(0) − 0.4. `clamp_past_bow_pulls_in`. `clamp_handles_tapered_bow`: at z = zMax − 0.1, the result is |x| ≤ halfBeam(z) − 0.4 or x == 0.
- [ ] Run → FAIL. Implement, then run → PASS.
- [ ] `main.cpp`: the mouse is captured, `1`/`2` switch the scene, and the cameras keep their state across switches.
- [ ] Commit "feat: fly and deck cameras with deck clamping".

### Task 4: Procedural galleon and textures

**Files:** Create `src/ship.{hpp,cpp}`, `src/textures.{hpp,cpp}`, `shaders/lit.{vert,frag}`, add tests

**Interfaces:**
- `struct GalleonParams { float length = 34, beam = 9, depth = 6; glm::vec3 hullColor, trimColor; int seed; };`
- `struct GalleonMeshes { Mesh hull, deck, wood, sails, rigging; std::vector<glm::vec3> lanternLocal; DeckOutline outline; };`. `wood` covers masts, yards, rails, castles, the wheel and barrels. `rigging` is drawn as GL_LINES.
- `GalleonGeometry buildGalleonGeometry(const GalleonParams&);` returns the CPU vertex and index arrays per part (for tests). `GalleonMeshes uploadGalleon(const GalleonGeometry&);`
- `GLuint makeWoodTexture(int seed); GLuint makeCanvasTexture(); GLuint makeNoiseTexture();`. These are 512² RGBA textures with mipmaps.

Hull: loft about 24 cross-sections along z. Each section is a curve with a flat bottom, rounded bilge and tumblehome (the hull narrows toward the top). The beam follows a smooth curve that tapers to the bow and goes square at the stern. The sheer line (the deck edge) rises at the bow and steeply at the stern castle. Gun-port rows are dark quads on the hull, stern windows are emissive quads (`color` > 1 in HDR), and the bowsprit is a cylinder. There are 3 masts (fore, main, mizzen) with 2–3 yards each. Sails are 12×12 grids with uv, slightly cambered. Rigging runs as shrouds from the mast tops to the rails.

- [ ] Tests: `galleon_nonempty`: every part has > 0 indices and every index is < its vertex count. `galleon_bounds`: the hull fits within length × beam (±10%). `galleon_normals_unit`. `deck_outline_matches_hull`: halfBeam(0) is within 10% of beam/2.
- [ ] Run → FAIL. Implement, then run → PASS.
- [ ] `lit` shader: Blinn-Phong with moonlight, ambient, emissive (from vertex colour > 1), a texture by material id, and a wetness term. It also has a `uPointLights[16]` array (pos, colour, radius), which is wired up later.
- [ ] Verify: `--shot ship.png --scene 2 --time 5` with the fly camera placed beside one ship. Inspect the image, and iterate on the hull shape until it reads clearly as a galleon.
- [ ] Commit "feat: procedural galleon geometry and textures".

### Task 5: Fleet riding the waves, deck walking

**Files:** Modify `src/ship.{hpp,cpp}` and `src/main.cpp`, add tests

**Interfaces:**
- `struct ShipState { glm::vec2 xz; float heading, speed; float sinkProgress = 0; float extraRoll = 0; };`
- `glm::mat4 shipPose(const ShipState&, float t, float length, float beam);`. It samples the wave height at the bow, stern, port and starboard and smooths the result: the y offset is the average, pitch = atan(Δbow/length), roll = atan(Δside/beam). `sinkProgress` (0..1) lowers the ship by up to 25 m and adds pitch, so the stern rises.
- The fleet is 6 ships in a loose formation around the origin, sailing at heading +X at 3 m/s. The world wraps at ±600 m (the far ocean edge is hidden by fog).

- [ ] Tests: `pose_follows_wave`: the translated y equals the waveSample height at xz within 0.6 m. `pose_rotation_orthonormal`. `pose_sinking_lowers`: sinkProgress 1 gives y < −20.
- [ ] Run → FAIL. Implement, then run → PASS.
- [ ] Scene 1 builds the view matrix from flagship model × the deck-camera local transform.
- [ ] Verify: shot `--scene 1` (a deck view with the other ships visible) and shot `--scene 2`. Inspect both.
- [ ] Commit "feat: floating fleet and deck walking". **Tier 1 complete.**

### Task 6: Sky, clouds, fog, HDR pipeline

**Files:** Create `src/sky.{hpp,cpp}`, `src/renderer.{hpp,cpp}`, `shaders/sky.*`, `shaders/post.*` (tonemap, bloom bright pass and blur)

**Interfaces:**
- `class Renderer { void init(int w,int h); void resize(int w,int h); void beginScene(); void endScene(float exposure, float flash); FrameUniforms& frame(); }`
- `struct FrameUniforms { glm::mat4 view, proj; glm::vec3 camPos, moonDir, moonColor, ambient, fogColor; float fogDensity, time, flash; std::vector<PointLight> lights; }`. This is uploaded through a UBO bound at 0, and every shader includes the same block.
- Sky: a fullscreen pass that reconstructs the view ray. It blends a gradient with three layers of fbm clouds that scroll with the wind. A dim moon glow shows through the gaps, and `flash` lights the clouds from inside.
- Fog: exponential with height falloff. It blends to `fogColor`, which matches the horizon colour of the sky.
- Bloom: a bright pass (threshold 1.0) with 5 levels of downsample and upsample, then ACES and gamma 2.2.

- [ ] Verify the shots in both scenes. Expected: no hard horizon line and a moody blue-grey palette.
- [ ] Commit "feat: sky, fog, HDR and bloom".

### Task 7: Lightning, flash, rain

**Files:** Create `src/weather.{hpp,cpp}`, `shaders/bolt.*`, `shaders/rain.*`, add tests

**Interfaces:**
- `struct Bolt { std::vector<std::pair<glm::vec3,glm::vec3>> segments; float age, life; };`
- `Bolt generateBolt(glm::vec3 from, glm::vec3 to, uint32_t seed, int depth = 6);`. This uses midpoint displacement with an offset of 0.25 × the segment length, and branches with probability 0.35 per level (each branch is 0.5× the length).
- `float flashEnvelope(float age);`. It returns 0..1 and flickers: three pulses within 0.35 s, then decays.
- `class Weather { void update(float dt, float t); void triggerStrike(glm::vec3 target); float flash() const; const std::vector<Bolt>& bolts() const; }`. There are random ambient strikes every 6–14 s at 300–900 m away.
- Rain: 20k streaks in a 60 m box around the camera, wrapped with mod in the vertex shader and stretched along the wind and fall velocity.

- [ ] Tests: `bolt_connects_endpoints`: the main path's first segment starts at `from` and its last ends at `to`. `bolt_deterministic`: the same seed gives the same segments. `flash_envelope_bounds`: the value is within [0,1], 0 at age 2, and > 0.5 at age 0.
- [ ] Run → FAIL. Implement, then run → PASS.
- [ ] Bolts are drawn as camera-facing quads with an HDR colour of about (6, 7, 10). The flash feeds `FrameUniforms.flash`, which raises the moon intensity ×8 and the ambient light ×4.
- [ ] Verify: `--time` set to a known strike time shows a bolt and a flash-lit scene.
- [ ] Commit "feat: lightning bolts, flash lighting, rain".

### Task 8: Strike event — fire, smoke, firelight, sinking

**Files:** Create `src/event.{hpp,cpp}`, `src/particles.{hpp,cpp}`, `shaders/particle.*`, add tests

**Interfaces:**
- `enum class Phase { Calm, Strike, Burning, Sinking, Sunk };`
- `struct StrikeEvent { float t0; Phase phase(float t) const; float sinkProgress(float t) const; float fireIntensity(float t) const; void retrigger(float now); }`. Timeline from t0: the strike is at +0, Burning runs +0.3..+40, Sinking runs +40..+90 (sinkProgress uses smoothstep), and Sunk is ≥ +90. Fire intensity ramps up over 8 s, holds, and fades out as the ship goes under. The first trigger is at t0 = 20 s after startup.
- `class Particles { void emit(const Emitter&, float dt); void update(float dt, glm::vec2 wind); void draw(const Shader&); void clear(); }`. Fire is additive, smoke is alpha-blended (sorted back to front), and embers are tiny and bright. There are at most 6000 particles. The emitters are attached in ship-local space at the main mast and deck.
- Firelight: 3 flickering point lights on the struck ship (colour (4, 1.6, 0.4) × intensity, radius 40 m), added to `FrameUniforms.lights`, together with the lanterns of every ship (dim, warm).
- The struck ship gets a charred tint that grows with the burn time (a uniform `uChar`). Its main-mast sails are hidden once the fire has burned for 25 s.

- [ ] Tests: `phases_in_order`: t0 + {−1, 0.1, 10, 50, 100} gives Calm, Strike, Burning, Sinking, Sunk. `sink_progress_monotonic`: the values in [0,1] never decrease. `retrigger_resets`: after a retrigger, the phase is Calm or Strike and sinkProgress is 0. `phase_scene_independent`: the phase is a pure function of t (there is no scene parameter by construction, and the test documents that).
- [ ] Run → FAIL. Implement, then run → PASS.
- [ ] Wire it up: at the Strike phase, `Weather::triggerStrike(main-mast-top of ship 3)`, and `R` retriggers it. On a retrigger, `Particles::clear()` runs and ship 3's pose resets.
- [ ] Verify: shots at t = 35 (burning, orange light on the water) and t = 80 (sinking, stern up) in scene 2, plus a scene-1 shot facing ship 3.
- [ ] Commit "feat: lightning strike event with fire, smoke and sinking". **Tier 2 complete.**

### Task 9 (Tier 3): Shadows, billowing sails, thunder, README

**Files:** Modify `renderer`, `lit.*`, `ship.cpp`. Create `src/audio.{hpp,cpp}` and `README.md`.

- [ ] Shadow map: 2048² from the moon direction, an orthographic frustum fitted to a 200 m box around the camera, and 3×3 PCF filtering. During a flash the shadows sharpen and darken.
- [ ] Sails billow in the vertex shader: the bulge is `uv`-based (zero at the edges), and it flutters with time and the wind gusts.
- [ ] Thunder: miniaudio playback of noise synthesized in code (low-passed brown noise with a crackle onset and a rumble that decays over 4–8 s). The delay is distance/343 m/s after the flash. If audio init fails, it fails silently.
- [ ] README: the build steps (WinLibs and CMake), the controls, a feature list with the graphics technique behind each feature (for the report or viva), and a note that everything is built in code.
- [ ] Final verification: a clean build from scratch, `tests` passes, 60 s of interactive running in both scenes at ≥ 30 fps (frame time printed in the window title), and final showcase shots.
- [ ] Commit "feat: shadows, billowing sails, thunder audio, README".
