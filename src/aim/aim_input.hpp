#pragma once

#include "aim_math.hpp"

namespace Aim {

void moveMouseForAngleDelta(const Vec3& deltaAngle, float sensitivity, float smoothFactor = 0.35f);

} // namespace Aim
