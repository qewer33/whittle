#pragma once

// axis-aligned hit rect in screen pixels
struct Rect
{
    int x, y, w, h;
    bool contains(int px, int py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

// cell i of a centered segmented switch (count cells). shared by draw and
// hit-test so they agree.
inline Rect segCell(int i, int count)
{
    constexpr int segW = 44, segPad = 3, barY = 240 - 22, barH = 22;
    const int x0 = 160 - count * segW / 2;
    return {x0 + i * segW, barY + segPad, segW, barH - 2 * segPad};
}
