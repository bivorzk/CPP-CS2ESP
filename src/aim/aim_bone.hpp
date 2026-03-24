#pragma once

#include "aim_math.hpp"
#include "mem.hpp"
#include "offsets.hpp"
#include "gui.hpp"

namespace Aim {

bool isPawnAlive(mem::ProcessMemory* proc, uintptr_t pawn);
std::vector<int> boneCandidatesFor(int aimPart);
bool isPawnVisible(mem::ProcessMemory* proc, uintptr_t pawn, int localPlayerId, bool strictMaskOnly = false);

bool getRawBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, int boneIndex, Vec3& out);
bool getBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, const Vec3& origin, int aimPart, Vec3& out);
Vec3 selectAimPoint(const Vec3& origin, const Vec3& viewOffset, int aimPart);

} // namespace Aim
