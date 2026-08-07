#pragma once

#include <3ds.h>
#include <math.h>

// Sweetie 16 (GrafxKid, CC0). face colors are raw RGBA, these are just the
// picker presets. stored as 0xRRGGBBAA.
inline constexpr int kPaletteCount = 16;
inline constexpr u32 kPalette[kPaletteCount] = {
    0x1a1c2cFF, 0x5d275dFF, 0xb13e53FF, 0xef7d57FF,
    0xffcd75FF, 0xa7f070FF, 0x38b764FF, 0x257179FF,
    0x29366fFF, 0x3b5dc9FF, 0x41a6f6FF, 0x73eff7FF,
    0xf4f4f4FF, 0x94b0c2FF, 0x566c86FF, 0x333c57FF,
};

// HSV (h,s,v in [0,1]) to 0xRRGGBBAA (opaque)
inline u32 hsv2rgb(float h, float s, float v)
{
    h -= floorf(h); // wrap
    const int i = (int)(h * 6.0f) % 6;
    const float f = h * 6.0f - floorf(h * 6.0f);
    const float p = v * (1.0f - s), q = v * (1.0f - f * s), t = v * (1.0f - (1.0f - f) * s);
    float r = v, g = t, b = p;
    if (i == 1) { r = q; g = v; b = p; }
    else if (i == 2) { r = p; g = v; b = t; }
    else if (i == 3) { r = p; g = q; b = v; }
    else if (i == 4) { r = t; g = p; b = v; }
    else if (i == 5) { r = v; g = p; b = q; }
    const u32 R = (u32)(r * 255.0f + 0.5f), G = (u32)(g * 255.0f + 0.5f), B = (u32)(b * 255.0f + 0.5f);
    return (R << 24) | (G << 16) | (B << 8) | 0xFF;
}

// 0xRRGGBBAA to HSV (h,s,v in [0,1])
inline void rgb2hsv(u32 c, float& h, float& s, float& v)
{
    const float r = ((c >> 24) & 0xFF) / 255.0f, g = ((c >> 16) & 0xFF) / 255.0f,
                b = ((c >> 8) & 0xFF) / 255.0f;
    const float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    const float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    const float d = mx - mn;
    v = mx;
    s = mx < 1e-6f ? 0.0f : d / mx;
    if (d < 1e-6f)
        h = 0.0f;
    else if (mx == r)
        h = (g - b) / d / 6.0f;
    else if (mx == g)
        h = (2.0f + (b - r) / d) / 6.0f;
    else
        h = (4.0f + (r - g) / d) / 6.0f;
    if (h < 0.0f)
        h += 1.0f;
}
