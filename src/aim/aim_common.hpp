#pragma once

#include <array>
#include <vector>
#include <cstdint>
#include <string>
#include <optional>
#include <windows.h>

#include "mem.hpp"
#include "offsets.hpp"
#include "gui.hpp"

namespace Aim {

struct Vec3 { float x, y, z; };
struct FovResult { float fovDeg; float distance; };

bool isCs2ForegroundWindow();
float clampf01(float v, float minv, float maxv);
float normalizeYaw(float yaw);
Vec3 calcAngle(const Vec3& src, const Vec3& dst);
Vec3 angleToDirection(const Vec3& ang);
float dotProduct(const Vec3& a, const Vec3& b);
float length(const Vec3& v);
Vec3 normalize(const Vec3& v);
FovResult GetTargetFOVAndDistance(const Vec3& eyePos, const Vec3& viewDir, const Vec3& targetPos);
float distance3d(const Vec3& a, const Vec3& b);

bool isPawnAlive(mem::ProcessMemory* proc, uintptr_t pawn);
std::vector<int> boneCandidatesFor(int aimPart);
bool isPawnVisible(mem::ProcessMemory* proc, uintptr_t pawn, int localPlayerId, bool strictMaskOnly = false);

bool worldToScreen(const Vec3& world, const std::array<float,16>& m, int width, int height, POINT& out);
bool getCS2WindowRect(RECT& out);
int getLocalPlayerId(mem::ProcessMemory* proc, uintptr_t entityList, uintptr_t localPawn);

bool getRawBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, int boneId, Vec3& out);
bool getBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, const Vec3& origin, int aimPart, Vec3& out);
Vec3 selectAimPoint(const Vec3& origin, const Vec3& viewOffset, int aimPart);

float calcAimbotScore(float fov, float dist, int health);
float angdiff(float a, float b);
Vec3 smoothAim(const Vec3& current, const Vec3& desired, float fraction);

void moveMouseForAngleDelta(const Vec3& delta, float sensitivity);

} // namespace Aim
