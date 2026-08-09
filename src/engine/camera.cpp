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

// deadzone plus normalize a raw stick reading to -1..1
static float stickAxis(float raw)
{
    static constexpr float kDeadzone = 12.0f;
    return fabsf(raw) > kDeadzone
               ? copysignf((fabsf(raw) - kDeadzone) / (CIRCLE_PAD_MAX - kDeadzone), raw)
               : 0.0f;
}

void Camera::orbit(const circlePosition& pad, float dt)
{
    if (dt > 0.1f)
        dt = 0.1f;
    yaw -= stickAxis((float)pad.dx) * kOrbitSpeed * dt;
    pitch += stickAxis((float)pad.dy) * kOrbitSpeed * dt;
    if (pitch > kMaxPitch)
        pitch = kMaxPitch;
    if (pitch < -kMaxPitch)
        pitch = -kMaxPitch;
}

void Camera::update(const circlePosition& pad, const circlePosition& cstick, u32 held, float dt)
{
    if (dt > 0.1f)
        dt = 0.1f;

    orbit(pad, dt); // circle pad

    // dpad pans the target in the camera screen plane
    float panX = 0.0f, panY = 0.0f;
    if (held & KEY_DLEFT)
        panX -= 1.0f;
    if (held & KEY_DRIGHT)
        panX += 1.0f;
    if (held & KEY_DUP)
        panY += 1.0f;
    if (held & KEY_DDOWN)
        panY -= 1.0f;
    if (panX != 0.0f || panY != 0.0f)
    {
        const Vec3 e = eye();
        const Vec3 fwd = {target.x - e.x, target.y - e.y, target.z - e.z};
        const float flen = sqrtf(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
        const Vec3 fn = {fwd.x / flen, fwd.y / flen, fwd.z / flen};
        Vec3 right = {-fn.z, 0.0f, fn.x};
        const float rlen = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
        right = {right.x / rlen, right.y / rlen, right.z / rlen};
        const Vec3 up = {
            right.y * fn.z - right.z * fn.y,
            right.z * fn.x - right.x * fn.z,
            right.x * fn.y - right.y * fn.x,
        };
        const float s = kPanSpeed * distance * dt;
        target.x += right.x * panX * s + up.x * panY * s;
        target.y += right.y * panX * s + up.y * panY * s;
        target.z += right.z * panX * s + up.z * panY * s;
    }

    // X/Y zoom at a fixed rate, C-stick zooms smoothly (New 3DS)
    float zoom = 0.0f;
    if (held & KEY_X)
        zoom += 1.0f;
    if (held & KEY_Y)
        zoom -= 1.0f;
    zoom += stickAxis((float)cstick.dy);
    if (zoom != 0.0f)
    {
        distance *= expf(-zoom * kZoomSpeed * dt);
        if (distance < kMinDistance)
            distance = kMinDistance;
        if (distance > kMaxDistance)
            distance = kMaxDistance;
    }
}

C3D_Mtx Camera::viewProj(float iod) const
{
    const Vec3 e = eye();

    C3D_Mtx view;
    Mtx_LookAt(&view,
               FVec4_New(e.x, e.y, e.z, 1.0f),
               FVec4_New(target.x, target.y, target.z, 1.0f),
               FVec4_New(0.0f, 1.0f, 0.0f, 1.0f),
               true);

    // stereo converges at the orbit target (distance away), so it sits at the
    // screen plane. iod 0 is the plain mono projection.
    C3D_Mtx proj;
    if (iod != 0.0f)
        Mtx_PerspStereoTilt(&proj, C3D_AngleFromDegrees(45.0f), 400.0f / 240.0f, 0.1f, 100.0f,
                            iod, distance, true);
    else
        Mtx_PerspTilt(&proj, C3D_AngleFromDegrees(45.0f), 400.0f / 240.0f, 0.1f, 100.0f, true);

    C3D_Mtx vp;
    Mtx_Multiply(&vp, &proj, &view);
    return vp;
}
