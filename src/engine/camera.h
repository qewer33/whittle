#pragma once

#include <3ds.h>
#include <citro3d.h>
#include "mesh.h"

// orbit camera: yaw/pitch/distance around a target
struct Camera
{
    float yaw = 0.75f;      // radians
    float pitch = 0.5f;     // radians
    float distance = 4.5f;
    Vec3 target = {0.0f, 0.0f, 0.0f};

    void update(const circlePosition& pad, const circlePosition& cstick, u32 held, float dt);
    void orbit(const circlePosition& pad, float dt); // circle pad only (2D preview)
    C3D_Mtx viewProj(float iod = 0.0f) const; // iod = stereo eye offset (0 = mono)
    Vec3 eye() const;

private:
    static constexpr float kOrbitSpeed = 2.4f;
    static constexpr float kPanSpeed = 1.1f;
    static constexpr float kZoomSpeed = 1.6f;
    static constexpr float kMinDistance = 0.5f;
    static constexpr float kMaxDistance = 40.0f;
    static constexpr float kMaxPitch = 1.5f;
};
