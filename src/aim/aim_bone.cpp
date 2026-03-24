#include "aim_bone.hpp"
#include "aim_math.hpp"
#include <array>

namespace Aim {

bool isPawnAlive(mem::ProcessMemory* proc, uintptr_t pawn) {
    if (!proc || !pawn) return false;
    auto aliveOpt = proc->read<uint8_t>(pawn + Offsets::m_bPawnIsAlive::STATIC_PTR);
    if (aliveOpt) return (*aliveOpt != 0);
    auto healthOpt = proc->read<int32_t>(pawn + Offsets::m_iPawnHealth::STATIC_PTR);
    return (healthOpt && *healthOpt > 0);
}

std::vector<int> boneCandidatesFor(int aimPart) {
    switch (aimPart) {
        case 0: return {6, 5};
        case 1: return {4, 2, 0, 5};
        case 2: return {8, 9, 11, 13, 14, 16};
        case 3: return {22, 23, 24, 25, 26, 27};
        default: return {6, 5, 4, 0};
    }
}

bool isPawnVisible(mem::ProcessMemory* proc, uintptr_t pawn, int localPlayerId, bool strictMaskOnly) {
    static bool g_forceVisible = true;
    if (g_forceVisible) return true;

    if (!proc || !pawn) return false;

    auto dormantOpt = proc->read<uint8_t>(pawn + Offsets::m_bDormant::STATIC_PTR);
    bool dormant = dormantOpt.has_value() && (*dormantOpt != 0);
    if (dormant) {
        Gui::log("[DBG] pawn=0x%llX visibility=fail dormant=1", (unsigned long long)pawn);
        return false;
    }

    auto spottedOpt = proc->read<uint8_t>(pawn + Offsets::m_bSpotted::STATIC_PTR);
    auto maskOpt = proc->read<uint32_t>(pawn + Offsets::m_bSpottedByMask::STATIC_PTR);

    bool hasSpotted = spottedOpt.has_value();
    bool hasMask = maskOpt.has_value();

    bool spotted = hasSpotted && (*spottedOpt != 0);
    bool spottedByLocal = false;
    bool localValid = (localPlayerId >= 0 && localPlayerId < 32);
    if (hasMask && localValid) spottedByLocal = (((*maskOpt) >> localPlayerId) & 1u) != 0;

    bool visible = false;
    if (strictMaskOnly) {
        if (hasMask && localValid) {
            visible = spottedByLocal;
            if (!visible) Gui::log("[DBG] pawn=0x%llX strict-fail", (unsigned long long)pawn);
        } else {
            visible = false;
        }
    } else {
        if (hasMask && localValid) visible = spottedByLocal;
        else if (hasSpotted) visible = spotted;
        else visible = true;
    }

    return visible;
}

bool getRawBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, int boneIndex, Vec3& out) {
    if (!proc || !pawn) return false;

    constexpr uintptr_t BONE_STRIDE = 0x20;

    auto gameSceneNodeOpt = proc->read<uintptr_t>(pawn + Offsets::m_gameSceneNode::STATIC_PTR);
    if (!gameSceneNodeOpt || *gameSceneNodeOpt == 0) return false;
    uintptr_t gameSceneNode = *gameSceneNodeOpt;

    uintptr_t boneArrayAddr = gameSceneNode + Offsets::m_modelState::STATIC_PTR + Offsets::boneArrayOffset::STATIC_PTR;
    auto boneArrayPtrOpt = proc->read<uintptr_t>(boneArrayAddr);
    if (!boneArrayPtrOpt || *boneArrayPtrOpt == 0) return false;
    uintptr_t boneArray = *boneArrayPtrOpt;

    uintptr_t entry = boneArray + static_cast<uintptr_t>(boneIndex) * BONE_STRIDE;
    auto bonePosOpt = proc->read<Vec3>(entry);
    if (!bonePosOpt) return false;

    out = *bonePosOpt;
    return true;
}

bool getBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, const Vec3& origin, int aimPart, Vec3& out) {
    for (int bone : boneCandidatesFor(aimPart)) {
        Vec3 candidate;
        if (!getRawBoneWorldPosition(proc, pawn, bone, candidate)) continue;
        if (distance3d(origin, candidate) > 150.0f) continue;
        out = candidate;
        return true;
    }
    return false;
}

Vec3 selectAimPoint(const Vec3& origin, const Vec3& viewOffset, int) {
    return {origin.x + viewOffset.x, origin.y + viewOffset.y, origin.z + viewOffset.z};
}

} // namespace Aim
