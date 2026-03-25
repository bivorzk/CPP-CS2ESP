#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include "aim_input.hpp"

namespace Aim {

void moveMouseForAngleDelta(const Vec3& deltaAngle, float sensitivity, float smoothFactor = 0.35f)
{
    if (std::fabs(deltaAngle.x) < 0.001f && std::fabs(deltaAngle.y) < 0.001f)
        return;

    const float m_yaw   = 0.022f;   // TODO: make configurable / read from game if possible
    const float m_pitch = 0.022f;

    float sens = sensitivity > 0.01f ? sensitivity : 1.0f;

    float pxPerDegYaw   = 1.0f / (sens * m_yaw);
    float pxPerDegPitch = 1.0f / (sens * m_pitch);

    float dxRaw = -deltaAngle.y * pxPerDegYaw   * smoothFactor;
    float dyRaw =  deltaAngle.x * pxPerDegPitch * smoothFactor;

    int dx = static_cast<int>(std::round(dxRaw));
    int dy = static_cast<int>(std::round(dyRaw));

    if (dx == 0 && std::fabs(deltaAngle.y) > 0.003f)
        dx = (deltaAngle.y > 0) ? -1 : 1;
    if (dy == 0 && std::fabs(deltaAngle.x) > 0.003f)
        dy = (deltaAngle.x > 0) ? 1 : -1;

    if (dx == 0 && dy == 0)
        return;

    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = dx;
    input.mi.dy = dy;

    SendInput(1, &input, sizeof(INPUT));
}

} // namespace Aim
