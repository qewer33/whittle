#pragma once

#include "viewport.h"
#include "renderer.h"
#include "mesh.h"

// object-transform gizmos drawn in the ortho viewports, at the selection centroid
namespace gizmo
{
    constexpr int kUniform = -2; // scaleHit result: the center (uniform) handle

    void drawMove(const Viewport& vp, const Vec3& centroid, Renderer& r);   // axis arrows
    void drawScale(const Viewport& vp, const Vec3& centroid, Renderer& r);  // axis handles + center
    void drawRotate(const Viewport& vp, const Vec3& centroid, Renderer& r); // a ring

    // world axis (vp.axisX / vp.axisY) whose handle the tap hits, or -1
    int moveHitAxis(const Viewport& vp, const Vec3& centroid, int px, int py);
    // world axis, kUniform, or -1
    int scaleHit(const Viewport& vp, const Vec3& centroid, int px, int py);
    bool rotateHitRing(const Viewport& vp, const Vec3& centroid, int px, int py);
}
