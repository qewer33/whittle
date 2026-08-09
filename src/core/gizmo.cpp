#include "gizmo.h"
#include <math.h>

namespace
{
    constexpr float kLenPx = 26.0f;  // move arrow length
    constexpr float kOffPx = 8.0f;   // move grab band starts this far from the center
    constexpr float kGrabPx = 7.0f;  // move grab half-thickness
    constexpr float kBarbPx = 5.0f;  // arrowhead barb length

    constexpr float kStubPx = 24.0f;    // scale handle offset from center
    constexpr float kSqPx = 4.0f;       // scale axis-handle square half-size
    constexpr float kCenterPx = 5.0f;   // scale center-handle square half-size
    constexpr float kHandlePx = 8.0f;   // scale grab half-size

    constexpr float kRingPx = 28.0f;  // rotate ring radius
    constexpr float kBandPx = 8.0f;   // rotate grab half-thickness
    constexpr int kRingSegs = 24;

    const u32 kAxisCol[3] = {0xF04747FF, 0x4FD16BFF, 0x4A8CF0FF}; // X red, Y green, Z blue
    const u32 kCenterCol = 0xC2C8D4FF;

    // world axis not spanned by the viewport (the axis it looks down)
    int perpAxis(const Viewport& vp) { return 3 - vp.axisX - vp.axisY; }

    // square outline centered at `c` in the vp plane, into out[0..3]
    void pushSquare(const Vec3& c, float half, int axX, int axY, Line* out)
    {
        const float ca = getAxis(c, axX), cb = getAxis(c, axY);
        const float dx[4] = {-1, 1, 1, -1}, dy[4] = {-1, -1, 1, 1};
        Vec3 corner[4];
        for (int k = 0; k < 4; k++)
        {
            corner[k] = c;
            setAxis(corner[k], axX, ca + dx[k] * half);
            setAxis(corner[k], axY, cb + dy[k] * half);
        }
        for (int k = 0; k < 4; k++)
            out[k] = {corner[k], corner[(k + 1) & 3]};
    }
}

void gizmo::drawMove(const Viewport& vp, const Vec3& centroid, Renderer& r)
{
    const float len = kLenPx / vp.scale;
    const float barb = kBarbPx / vp.scale;
    const C3D_Mtx m = vp.matrix();

    for (int e = 0; e < 2; e++)
    {
        const int axis = e == 0 ? vp.axisX : vp.axisY;
        const int other = e == 0 ? vp.axisY : vp.axisX;
        Vec3 tip = centroid;
        setAxis(tip, axis, getAxis(centroid, axis) + len);
        Vec3 b1 = tip, b2 = tip;
        setAxis(b1, axis, getAxis(tip, axis) - barb);
        setAxis(b2, axis, getAxis(tip, axis) - barb);
        setAxis(b1, other, getAxis(tip, other) + barb * 0.6f);
        setAxis(b2, other, getAxis(tip, other) - barb * 0.6f);
        const Line lines[3] = {{centroid, tip}, {tip, b1}, {tip, b2}};
        r.drawLineSet(lines, 3, m, kAxisCol[axis], 240, 320);
    }
}

int gizmo::moveHitAxis(const Viewport& vp, const Vec3& centroid, int px, int py)
{
    float wa, wb;
    if (!vp.tapToWorld(px, py, wa, wb))
        return -1;
    const float ca = getAxis(centroid, vp.axisX), cb = getAxis(centroid, vp.axisY);
    const float len = kLenPx / vp.scale, off = kOffPx / vp.scale, th = kGrabPx / vp.scale;
    if (wa >= ca + off && wa <= ca + len && fabsf(wb - cb) < th)
        return vp.axisX;
    if (wb >= cb + off && wb <= cb + len && fabsf(wa - ca) < th)
        return vp.axisY;
    return -1;
}

void gizmo::drawScale(const Viewport& vp, const Vec3& centroid, Renderer& r)
{
    const float stub = kStubPx / vp.scale, sq = kSqPx / vp.scale, csq = kCenterPx / vp.scale;
    const C3D_Mtx m = vp.matrix();

    for (int e = 0; e < 2; e++)
    {
        const int axis = e == 0 ? vp.axisX : vp.axisY;
        Vec3 tip = centroid;
        setAxis(tip, axis, getAxis(centroid, axis) + stub);
        Line lines[5];
        lines[0] = {centroid, tip};
        pushSquare(tip, sq, vp.axisX, vp.axisY, lines + 1);
        r.drawLineSet(lines, 5, m, kAxisCol[axis], 240, 320);
    }

    Line center[4];
    pushSquare(centroid, csq, vp.axisX, vp.axisY, center);
    r.drawLineSet(center, 4, m, kCenterCol, 240, 320);
}

int gizmo::scaleHit(const Viewport& vp, const Vec3& centroid, int px, int py)
{
    float wa, wb;
    if (!vp.tapToWorld(px, py, wa, wb))
        return -1;
    const float ca = getAxis(centroid, vp.axisX), cb = getAxis(centroid, vp.axisY);
    const float stub = kStubPx / vp.scale, h = kHandlePx / vp.scale;
    if (fabsf(wa - ca) < h && fabsf(wb - cb) < h)
        return kUniform;
    if (fabsf(wa - (ca + stub)) < h && fabsf(wb - cb) < h)
        return vp.axisX;
    if (fabsf(wa - ca) < h && fabsf(wb - (cb + stub)) < h)
        return vp.axisY;
    return -1;
}

void gizmo::drawRotate(const Viewport& vp, const Vec3& centroid, Renderer& r)
{
    const float R = kRingPx / vp.scale;
    const float ca = getAxis(centroid, vp.axisX), cb = getAxis(centroid, vp.axisY);
    Line lines[kRingSegs];
    Vec3 prev;
    for (int i = 0; i <= kRingSegs; i++)
    {
        const float a = 6.28318530718f * i / kRingSegs;
        Vec3 p = centroid;
        setAxis(p, vp.axisX, ca + R * cosf(a));
        setAxis(p, vp.axisY, cb + R * sinf(a));
        if (i > 0)
            lines[i - 1] = {prev, p};
        prev = p;
    }
    r.drawLineSet(lines, kRingSegs, vp.matrix(), kAxisCol[perpAxis(vp)], 240, 320);
}

bool gizmo::rotateHitRing(const Viewport& vp, const Vec3& centroid, int px, int py)
{
    float wa, wb;
    if (!vp.tapToWorld(px, py, wa, wb))
        return false;
    const float ca = getAxis(centroid, vp.axisX), cb = getAxis(centroid, vp.axisY);
    const float R = kRingPx / vp.scale, band = kBandPx / vp.scale;
    return fabsf(hypotf(wa - ca, wb - cb) - R) < band;
}
