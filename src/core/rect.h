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
