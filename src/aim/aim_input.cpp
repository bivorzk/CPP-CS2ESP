#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include "aim_input.hpp"

namespace Aim {

void moveMouseForAngleDelta(const Vec3& delta, float sensitivity) {
    const float M_YAW = 0.022f;
    float sens = sensitivity > 0.01f ? sensitivity : 1.0f;
    float pixels_per_deg = 1.0f / (sens * M_YAW);

    int dx = (int)std::round(-delta.y * pixels_per_deg);
    int dy = (int)std::round(delta.x * pixels_per_deg);

    if (dx == 0 && dy == 0) return;
    if (dx == 0 && std::fabs(delta.y) > 0.005f) dx = (delta.y > 0) ? -1 : 1;
    if (dy == 0 && std::fabs(delta.x) > 0.005f) dy = (delta.x > 0) ? 1 : -1;

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    SendInput(1, &input, sizeof(INPUT));
}

} // namespace Aim
