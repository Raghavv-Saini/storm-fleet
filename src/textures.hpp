#pragma once
#include <glad/glad.h>

// All textures are generated procedurally at startup (no image files).
GLuint makeWoodTexture(int seed);   // RGB = plank albedo multiplier, A = roughness-ish variation
GLuint makeCanvasTexture();         // RGB = woven canvas with weathering, A = 1
GLuint makeWaterDetailTexture();    // RGB = tileable normal map, A = foam/noise mask
