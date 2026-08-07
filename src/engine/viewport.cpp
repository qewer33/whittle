#include "viewport.h"

// C3D_FVec stores its members reversed ({w,z,y,x}), so use named access
static void setComp(C3D_FVec& v, int axis, float value)
{
    if (axis == 0)
        v.x = value;
    else if (axis == 1)
        v.y = value;
    else
        v.z = value;
}

// world point to NDC. the bottom framebuffer is rotated 90deg, so world axisY
// drives ndc x and axisX drives ndc y. r[0]=ndc x, r[1]=ndc y, r[3]=w=1.
C3D_Mtx Viewport::matrix() const
{
    C3D_Mtx m;
    for (int i = 0; i < 4; i++)
        m.r[i] = FVec4_New(0.0f, 0.0f, 0.0f, 0.0f);

    const float fx = 2.0f * scale / 240.0f;
    const float fy = 2.0f * scale / 320.0f;

    // where the rect midpoint lands in ndc
    const float tx = 1.0f - 2.0f * (y + h / 2.0f) / 240.0f;
    const float ty = 1.0f - 2.0f * (x + w / 2.0f) / 320.0f;

    // flip mirrors the horizontal (axisX) axis, r[1] normally negates it
    const float sx = flipped ? 1.0f : -1.0f;

    // map world center to the rect center so grid and mesh pan together
    setComp(m.r[0], axisY, fx);
    m.r[0].w = tx - fx * centerY;
    setComp(m.r[1], axisX, sx * fy);
    m.r[1].w = ty - sx * fy * centerX;

    // depth along the axis perpendicular to the view
    const int axisZ = 3 - axisX - axisY;
    const int eps = ((axisX - axisY) * (axisY - axisZ) * (axisZ - axisX)) / 2;
    const float farSign = (flipped ? -1.0f : 1.0f) * (float)eps;
    const float depthHalf = 256.0f;
    setComp(m.r[2], axisZ, farSign / (2.0f * depthHalf));
    m.r[2].w = -0.5f;

    m.r[3].w = 1.0f;
    return m;
}

bool Viewport::tapToWorld(int px, int py, float& wx, float& wy) const
{
    if (!contains(px, py))
        return false;
    const float sx = flipped ? -1.0f : 1.0f;
    wx = centerX + sx * (px - (x + w / 2)) / scale;
    wy = centerY - (py - (y + h / 2)) / scale;
    return true;
}
