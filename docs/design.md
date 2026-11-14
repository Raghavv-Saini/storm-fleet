# Storm Fleet — Design

Date: 2026-11-14 · Time budget: ~5 hours, built by Claude

## Intent

A computer graphics course project in modern OpenGL. It is graded on looking good:
high detail, strong lighting and shading, and animation. The theme is a fleet of
1700s-style sailing galleons ("Pirates of the Caribbean vibes", but all original
designs) caught in a stormy North Sea at night, with thunder and lightning.
Lightning strikes one ship, sets it on fire, and the ship sinks.

## Goals

- One executable with **two explorable scenes**, switched with a key:
  1. **Deck scene:** first-person on the flagship. Walk the deck with WASD and the
     mouse. The deck rolls and pitches with the waves, so the horizon tilts. You
     can't leave the ship (the walkable area is clamped to the deck outline). The
     rest of the fleet is visible around you.
  2. **Flying scene:** a free-fly camera (WASD, mouse, Space/Ctrl for up and down,
     Shift to go faster) that can roam around the fleet.
- **Both scenes show the same world and the same event**: a lightning strike, then
  fire, then listing, then sinking. It runs on a timeline, and a key can retrigger it.
- Everything is **built in code** (procedural geometry). No external 3D models.
  Textures are also generated in code (procedural wood grain and canvas noise), so
  there are no asset files to manage.

## Non-goals / anti-goals

- No copying of any film's ships, flags or characters.
- No physics engine. Ship motion is computed by sampling the wave function.
- Failure, even if it "works": a flat, evenly lit scene, a frame rate below about
  30 fps on the GTX 1650 Ti, or a build that the user can't reproduce.

## Constraints

- Hardware: NVIDIA GTX 1650 Ti (plus AMD integrated graphics). The NVIDIA GPU is
  forced via the exported `NvOptimusEnablement` / `AmdPowerXpressRequestHighPerformance`
  symbols.
- Installed toolchain: MinGW.org GCC 6.3 (32-bit, 2016), which is too old. Plan:
  install a modern 64-bit GCC (MSYS2 UCRT64 or w64devkit) plus CMake, **with the
  user's approval**.
- Modern OpenGL **3.3 core**, C++17.
- Libraries: GLFW (window and input), GLAD (GL loader), GLM (math), all
  fetched/vendored into the project. Optional: miniaudio for thunder, with the sound
  itself synthesized in code from filtered noise.

## Approaches considered (ship geometry)

- **A. Everything built in code (CHOSEN by user).** This shows the most graphics
  skill, is fully original, and is easy to animate (billowing sails, swaying
  rigging, a burning and sinking ship).
- B. Load free 3D models via Assimp. Most detail for the least effort, but risky
  under course rules and harder to animate.
- C. Hybrid. Has the same rule risk as B.

## The world

- **Ocean:** a large grid mesh with a sum of ~6 **Gerstner waves** computed in the
  vertex shader, plus a camera-centred grid with level of detail.
  Shading: Fresnel reflection of the stormy sky, subsurface tint on the crests,
  foam where the waves are steepest, specular highlights from lightning, and
  firelight reflected on the water. CPU-side `waveHeight(x,z,t)` and normal
  functions use the same formula, so the ships float correctly.
- **Galleons (×6, flagship plus 5):** procedural hull lofted from cross-section
  curves (tumblehome, raised stern castle, forecastle), plank texture, gun ports,
  3 masts with yards, bowsprit, square sails as cloth grids that billow in the
  vertex shader with the wind, rigging lines (shrouds and stays), lanterns
  (point lights), stern windows with warm glow, and the railings and deck details
  (wheel, hatches, barrels) that matter for the deck scene. The ships vary in
  scale and colour.
- **Ship motion:** each ship samples the wave height and normal at its bow, stern
  and both sides to get heave, pitch and roll, and sails slowly along a course.
- **Sky:** a dark gradient dome with procedural layered noise clouds that scroll.
  Lightning brightens the clouds from the inside.
- **Lightning:** a branching bolt generated as a polyline (midpoint displacement)
  and drawn as glowing quads. It comes with a global flash (a short burst of
  directional light plus brighter ambient light) and thunder after a delay.
- **Rain:** GPU particles, drawn as streaks that move with the camera.
- **Fog:** exponential height fog that blends into the sky colour.
- **The event:** lightning hits the main mast of ship #3, which leaves a scorch.
  Fire particles (additive, billboards) and smoke particles (alpha-blended) spawn
  on the mast and deck. Flickering orange point lights cast light onto the ship,
  the water and nearby ships. After ~40 s the ship lists, its stern rises, and it
  sinks below the waves. Timeline: R retriggers it.

## Lighting and rendering

- Forward renderer with HDR framebuffer → **bloom** (lanterns, fire, lightning) →
  ACES tone mapping → gamma correction.
- Lights: dim moonlight (directional) with a **shadow map**, lightning flash (the
  same directional light, briefly intense), up to ~16 point lights (lanterns and
  fire) in a uniform array.
- Blinn-Phong (or a simple GGX) shading with normal-perturbed wood and wet
  surfaces (rain makes everything slightly shinier).

## Controls

- `1` / `2`: deck scene / flying scene
- WASD + mouse: move and look. In the flying scene, Space/Ctrl move up and down
  and Shift goes faster.
- `R`: retrigger the lightning-strike event. `L`: manual lightning flash.
- `F1`: show the controls overlay in the window title. `Esc`: quit.

## Priority tiers (5-hour budget)

1. **Must:** toolchain, window, both cameras, Gerstner ocean, procedural galleons
   floating on the waves, sky, fog, lighting, deck walking.
2. **Should:** lightning bolt and flash, rain, fire, smoke, firelight, sinking
   timeline, bloom.
3. **Nice:** shadow map, billowing sails, synthesized thunder audio, foam detail.

## Left to the builder

The exact wave parameters, the colour palette, the hull curve numbers, the shader
structure, and the file layout. One `README.md` covers the build and the controls.
