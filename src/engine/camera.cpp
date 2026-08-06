#include "camera.h"
#include <math.h>

#define CIRCLE_PAD_MAX 0x9C

Vec3 Camera::eye() const
{
    const float cp = cosf(pitch);
    return {
        target.x + distance * cp * sinf(yaw),
        target.y + distance * sinf(pitch),
        target.z + distance * cp * cosf(yaw),
    };
}

void Camera::update(const circlePosition& pad, u32 held, float dt)
{
    if (dt > 0.1f)
        dt = 0.1f;

    // circle pad rarely rests at exactly (0,0), so deadzone and rescale
    static constexpr float kDeadzone = 12.0f;
    const float rawX = (float)pad.dx;
    const float rawY = (float)pad.dy;
    float nx = 0.0f;
    float ny = 0.0f;
    if (fabsf(rawX) > kDeadzone)
        nx = copysignf((fabsf(rawX) - kDeadzone) / (CIRCLE_PAD_MAX - kDeadzone), rawX);
    if (fabsf(rawY) > kDeadzone)
        ny = copysignf((fabsf(rawY) - kDeadzone) / (CIRCLE_PAD_MAX - kDeadzone), rawY);

    if (held & KEY_L)
    {
        // pan: move the target in the camera plane
        const Vec3 e = eye();
        const Vec3 fwd = {
            target.x - e.x,
            target.y - e.y,
            target.z - e.z,
        };
        const float flen = sqrtf(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
        const Vec3 fn = {fwd.x / flen, fwd.y / flen, fwd.z / flen};

        // right = normalize(cross(fn, worldUp))
        Vec3 right = {-fn.z, 0.0f, fn.x};
        const float rlen = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
        right = {right.x / rlen, right.y / rlen, right.z / rlen};

        const Vec3 up = {
            right.y * fn.z - right.z * fn.y,
            right.z * fn.x - right.x * fn.z,
            right.x * fn.y - right.y * fn.x,
        };

        const float s = kPanSpeed * distance * dt;
        target.x += right.x * nx * s + up.x * ny * s;
        target.y += right.y * nx * s + up.y * ny * s;
        target.z += right.z * nx * s + up.z * ny * s;
    }
    else if (held & KEY_R)
    {
        // zoom: push up to get closer
        distance *= expf(-ny * kZoomSpeed * dt);
        if (distance < kMinDistance)
            distance = kMinDistance;
        if (distance > kMaxDistance)
            distance = kMaxDistance;
    }
    else
    {
        // orbit
        yaw -= nx * kOrbitSpeed * dt;
        pitch += ny * kOrbitSpeed * dt;
        if (pitch > kMaxPitch)
            pitch = kMaxPitch;
        if (pitch < -kMaxPitch)
            pitch = -kMaxPitch;
    }

    // D-pad up/down zooms without needing a trigger, so the view is usable with
    // the console flat on a desk
    if (held & (KEY_DUP | KEY_DDOWN))
    {
        const float dz = (held & KEY_DUP) ? 1.0f : -1.0f;
        distance *= expf(-dz * kZoomSpeed * dt);
        if (distance < kMinDistance)
            distance = kMinDistance;
        if (distance > kMaxDistance)
            distance = kMaxDistance;
    }
}

C3D_Mtx Camera::viewProj() const
{
    const Vec3 e = eye();

    C3D_Mtx view;
    Mtx_LookAt(&view,
               FVec4_New(e.x, e.y, e.z, 1.0f),
               FVec4_New(target.x, target.y, target.z, 1.0f),
               FVec4_New(0.0f, 1.0f, 0.0f, 1.0f),
               true);

    C3D_Mtx proj;
    Mtx_PerspTilt(&proj, C3D_AngleFromDegrees(45.0f), 400.0f / 240.0f, 0.1f, 100.0f, true);

    C3D_Mtx vp;
    Mtx_Multiply(&vp, &proj, &view);


    return vp;
}
