#include "aim_math.hpp"
#include <cmath>
#include <algorithm>

namespace Aim {


constexpr float M_PI = 3.14159265358979323846f;

float clampf01(float v, float minv, float maxv) {
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

float normalizeYaw(float yaw) {
    while (yaw > 180.0f) yaw -= 360.0f;
    while (yaw < -180.0f) yaw += 360.0f;
    return yaw;
}

Vec3 calcAngle(const Vec3& src, const Vec3& dst) {
    Vec3 delta{dst.x - src.x, dst.y - src.y, dst.z - src.z};
    float hyp = std::sqrt(delta.x * delta.x + delta.y * delta.y);

    Vec3 ang;
    ang.x = -std::atan2(delta.z, hyp) * (180.0f / M_PI);
    ang.y = std::atan2(delta.y, delta.x) * (180.0f / M_PI);
    ang.z = 0.0f;

    ang.x = clampf01(ang.x, -89.0f, 89.0f);
    ang.y = normalizeYaw(ang.y);
    return ang;
}

Vec3 angleToDirection(const Vec3& ang) {
    float radPitch = ang.x * (M_PI / 180.0f);
    float radYaw = ang.y * (M_PI / 180.0f);
    return { std::cos(radPitch) * std::cos(radYaw),
             std::cos(radPitch) * std::sin(radYaw),
             -std::sin(radPitch) };
}

float dotProduct(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length(const Vec3& v) {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

Vec3 normalize(const Vec3& v) {
    float len = length(v);
    if (len <= 1e-6f) return {0.0f,0.0f,0.0f};
    return {v.x/len, v.y/len, v.z/len};
}

FovResult GetTargetFOVAndDistance(const Vec3& eyePos, const Vec3& viewDir, const Vec3& targetPos) {
    Vec3 forward = normalize(viewDir);
    Vec3 toTarget{targetPos.x - eyePos.x, targetPos.y - eyePos.y, targetPos.z - eyePos.z};
    float distance = length(toTarget);
    if (distance <= 0.001f) return {180.0f, 0.0f};
    Vec3 toTargetN = normalize(toTarget);
    float dot = std::clamp(dotProduct(forward, toTargetN), -1.0f, 1.0f);
    float fovDeg = std::acos(dot) * (180.0f / M_PI);
    return {fovDeg, distance};
}

float distance3d(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

float angdiff(float a, float b) {
    float diff = a - b;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return diff;
}

float calcAimbotScore(float fov, float dist, uint32_t health) {
    return fov * 0.7f + dist * 0.2f - std::clamp<float>(static_cast<float>(health), 0.0f, 100.0f) * 0.05f;
}

Vec3 smoothAim(const Vec3& current, const Vec3& target, float factor) {
    return { current.x + (target.x - current.x) * factor,
             current.y + (target.y - current.y) * factor,
             0.0f };
}

} // namespace Aim
