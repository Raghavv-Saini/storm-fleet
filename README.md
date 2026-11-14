# Storm Fleet

A fleet of 1700s-style sailing galleons caught in a night storm in the North Sea.
Lightning strikes one ship, which catches fire and sinks. Written in **C++17 with
modern OpenGL 3.3 (core profile)**. Every ship, sail, rope and texture is generated
in code: there are **no model or image files** in the project.

You can explore it in two scenes:

| Key | Scene |
|-----|-------|
| `1` | **Deck** – walk the flagship (WASD + mouse). The deck rolls and pitches with the waves. You can't leave the ship or walk through masts, guns, hatches or the capstan. |
| `2` | **Flying** – free camera over the sea and fleet. |

## Controls

| Input | Action |
|-------|--------|
| `W A S D` + mouse | move / look |
| `Space` / `C` (or `Ctrl`) | up / down (flying scene) |
| `Shift` | move fast |
| `R` | replay the lightning-strike → fire → sinking event |
| `L` | trigger a lightning bolt |
| `M` | mute |
| `F1` | hide/show the controls in the window title |
| `Esc` | quit |

The strike happens ~20 s after start. The ship burns for 40 s, then sinks over 50 s.
The window title shows the fps and the phase (calm / STRIKE / burning / sinking / sunk).

## Building (Windows)

Requirements: a 64-bit MinGW-w64 GCC (tested with WinLibs GCC 16.2), CMake ≥ 3.20, git,
and a GPU/driver with OpenGL 3.3.

```
winget install BrechtSanders.WinLibs.POSIX.UCRT Kitware.CMake
cmake -B build -G "MinGW Makefiles"
cmake --build build -j
build\stormfleet.exe
```

GLFW and GLM are downloaded by CMake on first configure. GLAD, stb_image_write and
miniaudio are vendored under `external/`. Shaders are copied next to the executable.

Tests (pure CPU logic: waves, ship pose, deck collision, lightning, event timeline):

```
cmake --build build --target tests && build\tests.exe
```

Screenshot / benchmark options: `stormfleet --shot out.png --scene 2 --time 35`
(optionally `--cam x y z yaw pitch` or `--deck x z yaw pitch`) renders one frame at a
chosen moment of the simulation; `--bench 10` runs the live loop for 10 s and prints fps.

## What's in it, and the graphics technique behind each part

| Feature | Technique | Where |
|---------|-----------|-------|
| Ocean | Sum of 6 **Gerstner waves** in the vertex shader (real deep-water dispersion), camera-following grid with distance-based vertex spacing (0.5 m near, 2.5 km at the horizon). Fresnel reflection of the sky, subsurface tint on crests, foam from the Jacobian of the wave field (where crests pinch), two scrolling detail normal maps, specular glints from moon and lightning. | `ocean_core.cpp`, `shaders/ocean.*` |
| Ships float correctly | The *same* wave formula runs on the CPU; each hull samples height at bow, stern and both sides to get heave, pitch and roll. | `ship_core.cpp: shipPose` |
| Galleons | **Procedural lofted hull** from cross-section curves (tumblehome, raised stern castle and forecastle), gun ports, stern gallery windows, masts with tops and yards, cambered sail grids, bowsprit and jibs, shrouds with ratlines, wheel, hatches, capstan, barrels, cannons, lanterns. | `ship_core.cpp` |
| Textures | **Procedural**: plank wood with grain, butt joints and nail heads; woven canvas with seams and stains; tileable water normal map – all from noise and sine functions at startup. | `textures.cpp` |
| Sails move | Vertex-shader **billow and flutter** driven by time and wind gusts. | `shaders/lit.vert` |
| Lighting | Blinn-Phong with wet-surface response (rain makes wood darker and glossier), moon directional light, hemisphere ambient, up to 16 **point lights** (lanterns, fire) chosen by distance. | `shaders/lit.frag`, `common.glsl` |
| Shadows | 2048² **shadow map** from the moon, orthographic frustum fitted around the camera, 3×3 **PCF**. | `renderer.cpp`, `main.cpp` |
| Sky | Fullscreen procedural sky: gradient, scrolling **fbm clouds**, moon disc seen through thin cloud, clouds lit from inside by lightning. | `shaders/sky.frag` |
| Fog | Exponential **height fog** blended to the horizon colour. | `common.glsl` |
| Lightning | **Midpoint-displacement** bolt with random branches, drawn as camera-facing glowing ribbons; a flicker envelope drives a global flash that brightens the moon light, ambient light, clouds and fog, and swings the light direction toward the strike. | `weather_core.cpp`, `weather.cpp` |
| Rain | 24 000 GPU-wrapped streaks that follow the camera. | `shaders/rain.*` |
| Fire & smoke | CPU **particle system** (6 000 max): stretched additive flame billboards, alpha-blended smoke sorted back-to-front, embers; flickering point lights light the ship, sea and neighbouring ships. Sails burn away and wood chars over time. | `particles.cpp`, `main.cpp: emitFire` |
| Sinking | Ship pose lowers, pitches bow-down and heels; fire dies as it goes under. Timeline is a pure function of time, so both scenes always agree. | `event.cpp` |
| Post-processing | **4× MSAA** HDR (RGBA16F) target → resolve → **bloom** (bright pass + 6-level down/up-sample chain) → **ACES tone mapping** with exposure dip on flashes, chromatic aberration, vignette, dither. | `renderer.cpp`, `shaders/tonemap.frag` |
| Thunder | **Synthesized in code**: brown noise through a decaying low-pass with a short crack onset; delayed by distance. Rain bed from filtered noise. | `audio.cpp` |

## Project layout

```
src/        core logic (ocean_core, ship_core, camera, weather_core, event) has no GL calls
            and is unit-tested; the rest is OpenGL (renderer, ocean, ship, sky, weather, particles)
shaders/    GLSL 3.30; common.glsl holds the shared frame uniform block
tests/      assert-based tests for the GL-free core
docs/       design.md and the implementation plan
```
