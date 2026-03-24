#pragma once

#include <array>
#include <cstdint>

namespace Aim {

struct Vec3 { float x, y, z; };
struct FovResult { float fovDeg; float distance; };

float clampf01(float v, float minv, float maxv);
float normalizeYaw(float yaw);
Vec3 calcAngle(const Vec3& src, const Vec3& dst);
Vec3 angleToDirection(const Vec3& ang);
float dotProduct(const Vec3& a, const Vec3& b);
float length(const Vec3& v);
Vec3 normalize(const Vec3& v);
FovResult GetTargetFOVAndDistance(const Vec3& eyePos, const Vec3& viewDir, const Vec3& targetPos);
float distance3d(const Vec3& a, const Vec3& b);
float angdiff(float a, float b);
float calcAimbotScore(float fov, float dist, uint32_t health);
Vec3 smoothAim(const Vec3& current, const Vec3& target, float factor);

} // namespace Aim
