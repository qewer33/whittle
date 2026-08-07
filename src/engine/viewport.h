#pragma once

#include <3ds.h>
#include <citro3d.h>
#include "mesh.h"

// One ortho editing view on the bottom screen. Maps two world axes onto the
// rect's x/y (y-down).
struct Viewport
{
    int x, y, w, h;        // pixel rect (y-down)
    int axisX, axisY;      // world axes (0=x,1=y,2=z) shown as screen x, y
    float centerX, centerY;// world coords at rect center
    float scale;           // pixels per world unit
    bool flipped;          // view from the opposite side

    C3D_Mtx matrix() const;

    // screen px to world coords of the two mapped axes, false if outside
    bool tapToWorld(int px, int py, float& wx, float& wy) const;

    float spanX() const { return (float)w / scale; }
    float spanY() const { return (float)h / scale; }

    bool contains(int px, int py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};
