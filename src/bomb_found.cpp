#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "bomb_found.hpp"
#include "gui.hpp"

#include <vector>
#include <cmath>

namespace Bomb {

Info Finder::read(mem::ProcessMemory* proc) {
    Info info;
    if (!proc) return info;

    uintptr_t base = proc->getModuleBase(Offsets::MODULE);
    if (base == 0) {
        Gui::log("[Bomb] module base not found: %s", Offsets::MODULE);
        return info;
    }

    uintptr_t addr = base + PlantedC4Offset;
    auto plantedListOpt = proc->read<uintptr_t>(addr);
    if (!plantedListOpt || *plantedListOpt == 0) {
        Gui::log("[Bomb] dwPlantedC4 read failed @ %s + 0x%llX / 0x%llX", Offsets::MODULE,
                  (unsigned long long)PlantedC4Offset, (unsigned long long)addr);
        return info;
    }

    info.plantedC4ClassPointer = *plantedListOpt;

    auto readFloat = [&](uintptr_t base, uintptr_t off) -> std::optional<float> {
        return proc->read<float>(base + off);
    };

    auto readBool = [&](uintptr_t base, uintptr_t off) -> std::optional<bool> {
        auto v = proc->read<uint8_t>(base + off);
        if (!v) return std::nullopt;
        return (*v != 0);
    };

    auto readPos = [&](uintptr_t base, uintptr_t off) -> std::optional<Info::Vec3> {
        return proc->read<Info::Vec3>(base + off);
    };

    auto getGameTime = [&]() -> float {
        auto localPawnOpt = proc->read<uintptr_t>(base + Offsets::dwLocalPlayerPawn::STATIC_PTR);
        if (localPawnOpt && *localPawnOpt != 0) {
            auto simTimeOpt = proc->read<float>(*localPawnOpt + Offsets::m_flSimulationTime::STATIC_PTR);
            if (simTimeOpt) {
                return *simTimeOpt;
            }
        }
        return (float)(GetTickCount64() / 1000.0);
    };

    auto gameTime = getGameTime();

    auto isValidWorldCoordinate = [&](const Info::Vec3 &v, bool strict = true) {
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) return false;
        if (fabs(v.x) > 20000.0f || fabs(v.y) > 20000.0f || fabs(v.z) > 5000.0f) return false;

        if (strict) {
            if (fabs(v.x) < 20.0f && fabs(v.y) < 20.0f) return false;  // avoid local 0,0 lookups
            if (fabs(v.x) < 2.0f || fabs(v.y) < 2.0f) return false;      // avoid axis-aligned artifacts
            // Allow low Z values (bomb on ground can be z<=2), only reject ridiculous near-flat origin
            if (fabs(v.z) < 0.25f) return false;
        } else {
            // Relaxed but still reject trivial axis-doomed coords
            if (fabs(v.x) < 2.0f || fabs(v.y) < 2.0f) return false;
        }

        return true;
    };

    auto resolveBombTargetOrigin = [&](uintptr_t entity) -> std::optional<Info::Vec3> {
        auto localPlayerEntityList = proc->read<uintptr_t>(base + Offsets::dwEntityList::STATIC_PTR);
        if (!localPlayerEntityList || *localPlayerEntityList == 0) return std::nullopt;

        auto resolveHandle = [&](uint32_t handle)->std::optional<Info::Vec3> {
            if (handle == 0 || handle == 0xFFFFFFFF) return std::nullopt;
            uint32_t pawnIndex = handle & 0x7FFF;
            uint32_t listIdx = pawnIndex >> 9;
            uint32_t innerIdx = pawnIndex & 0x1FF;

            auto listEntry2Opt = proc->read<uintptr_t>(*localPlayerEntityList + 0x8 * listIdx + Offsets::EntityList::ENTRY_OFFSET);
            if (!listEntry2Opt || *listEntry2Opt == 0) return std::nullopt;
            auto pawnOpt = proc->read<uintptr_t>(*listEntry2Opt + Offsets::EntityList::CHUNK_STRIDE * innerIdx);
            if (!pawnOpt || *pawnOpt == 0) return std::nullopt;
            uintptr_t pawn = *pawnOpt;
            auto origin = proc->read<Info::Vec3>(pawn + Offsets::m_vecOrigin::STATIC_PTR);
            if (!origin) origin = proc->read<Info::Vec3>(pawn + Offsets::m_vecOrigin::ABS_ORIGIN);
            if (origin && isValidWorldCoordinate(*origin, true)) return origin;
            return std::nullopt;
        };

        auto defuserHandleOpt = proc->read<uint32_t>(entity + Offsets::C_PlantedC4::m_hBombDefuser);
        if (defuserHandleOpt) {
            if (auto defuserOrigin = resolveHandle(*defuserHandleOpt)) {
                Gui::log("[Bomb] resolved origin via m_hBombDefuser = %.2f,%.2f,%.2f", defuserOrigin->x, defuserOrigin->y, defuserOrigin->z);
                return defuserOrigin;
            }
        }

        return std::nullopt;
    };

    auto scanForOrigin = [&](uintptr_t base) -> std::optional<Info::Vec3> {
        // Prefer standard entity scene-node origin chain where available
        auto sceneNodePtrOpt = proc->read<uintptr_t>(base + Offsets::C_BaseEntity::m_pGameSceneNode);
        Gui::log("[Bomb] scanForOrigin entity=0x%llX m_pGameSceneNode=0x%llX", (unsigned long long)base,
                  (unsigned long long)(sceneNodePtrOpt ? *sceneNodePtrOpt : 0ULL));
        if (sceneNodePtrOpt && *sceneNodePtrOpt) {
            uintptr_t sceneNode = *sceneNodePtrOpt;
            auto absOriginOpt = proc->read<Info::Vec3>(sceneNode + Offsets::CGameSceneNode::m_vecAbsOrigin);
            Gui::log("[Bomb] sceneNode=0x%llX m_vecAbsOrigin=%.2f,%.2f,%.2f", (unsigned long long)sceneNode,
                      absOriginOpt ? absOriginOpt->x : 0.0f,
                      absOriginOpt ? absOriginOpt->y : 0.0f,
                      absOriginOpt ? absOriginOpt->z : 0.0f);
            if (absOriginOpt && isValidWorldCoordinate(*absOriginOpt, true)) {
                Gui::log("[Bomb] origin from CGameSceneNode m_vecAbsOrigin = %.2f,%.2f,%.2f", absOriginOpt->x, absOriginOpt->y, absOriginOpt->z);
                return absOriginOpt;
            }
            auto originOpt = proc->read<Info::Vec3>(sceneNode + Offsets::CGameSceneNode::m_vecOrigin);
            Gui::log("[Bomb] sceneNode=0x%llX m_vecOrigin=%.2f,%.2f,%.2f", (unsigned long long)sceneNode,
                      originOpt ? originOpt->x : 0.0f,
                      originOpt ? originOpt->y : 0.0f,
                      originOpt ? originOpt->z : 0.0f);
            if (originOpt && isValidWorldCoordinate(*originOpt, true)) {
                Gui::log("[Bomb] origin from CGameSceneNode m_vecOrigin = %.2f,%.2f,%.2f", originOpt->x, originOpt->y, originOpt->z);
                return originOpt;
            }
        }

        // Try direct C_PlantedC4 'spectate pos' (likely actual bomb world position in CS2)
        auto explodePosOpt = proc->read<Info::Vec3>(base + Offsets::C_PlantedC4::m_vecC4ExplodeSpectatePos);
        if (explodePosOpt && isValidWorldCoordinate(*explodePosOpt, true)) {
            Gui::log("[Bomb] origin from m_vecC4ExplodeSpectatePos = %.2f,%.2f,%.2f", explodePosOpt->x, explodePosOpt->y, explodePosOpt->z);
            return explodePosOpt;
        }

        // If we have missed it, check via related entity handles (only defuser path kept)
        auto targetOrigin = resolveBombTargetOrigin(base);
        if (targetOrigin) return targetOrigin;

        return std::nullopt;
    };

    // try the direct plantedC4 list pointer itself
    auto evaluateCandidate = [&](uintptr_t entity)->std::optional<Info> {
        Gui::log("[Bomb] evaluating candidate entity=0x%llX", (unsigned long long)entity);
        Info candidate;
        candidate.plantedC4ClassPointer = entity;

        auto originC = scanForOrigin(entity);
        if (!originC) {
            Gui::log("[Bomb] origin scan failed for entity=0x%llX", (unsigned long long)entity);
            return std::nullopt;
        }

        candidate.origin = *originC;
        candidate.valid = true;

        auto bTickingC = readBool(entity, Offsets::C_PlantedC4::m_bBombTicking);
        auto bActivatedC = readBool(entity, Offsets::C_PlantedC4::m_bC4Activated);
        auto bBeingDefused = readBool(entity, Offsets::C_PlantedC4::m_bBeingDefused);
        auto nBombSite = proc->read<int32_t>(entity + Offsets::C_PlantedC4::m_nBombSite);
        auto bDefused = readBool(entity, Offsets::C_PlantedC4::m_bBombDefused);
        auto blowC    = readFloat(entity, Offsets::C_PlantedC4::m_flC4Blow);

        if (blowC) candidate.blowTime = *blowC;

        bool timerOk = false;
        if (blowC) {
            float bt = *blowC;
            if (bt > 0.0f && bt < 180.0f) {
                // If the value is already a remaining countdown, trust it.
                timerOk = true;
            } else if (bt > gameTime + 0.5f && bt < gameTime + 180.0f) {
                // If the value is absolute game time (older style), compare to game clock.
                timerOk = true;
            }
        }

        bool isPlanting = bTickingC.value_or(false);
        bool isActivated = bActivatedC.value_or(false);
        bool isBeingDefused = bBeingDefused.value_or(false);
        bool hasBombSite = nBombSite && *nBombSite >= 0;
        bool isDefused = bDefused.value_or(false);

        // planted C4 should be active when planted/site/ticking/activated; ignore defused/exploded
        if (isDefused || isBeingDefused) {
            candidate.active = false;
        } else if (isPlanting || hasBombSite || isActivated) {
            candidate.active = true;
        } else if (blowC && timerOk) {
            candidate.active = true;
        } else if (blowC && *blowC > 0.0f && *blowC < 180.0f) {
            // fallback: preserve valid cook timer visibility if we can't correctly resolve active flags
            candidate.active = true;
        } else {
            candidate.active = false;
        }

        // best effort: if we found a nonzero position from the bomb entity, assume it's active even if site / timing fields are stale
        if (!candidate.active && originC && isValidWorldCoordinate(*originC, false) && !isDefused) {
            candidate.active = true;
        }

        Gui::log("[Bomb] candidate entity=0x%llX origin=%.2f,%.2f,%.2f ticking=%d activated=%d site=%d beingDefused=%d defused=%d blow=%.3f timerOk=%d active=%d",
                  (unsigned long long)entity,
                  candidate.origin.x, candidate.origin.y, candidate.origin.z,
                  bTickingC.value_or(false), isActivated, nBombSite.value_or(-1), isBeingDefused, isDefused, candidate.blowTime,
                  (int)timerOk, (int)candidate.active);

        return candidate;
    };

    uintptr_t listPtr = info.plantedC4ClassPointer;
    std::optional<Info> bestCandidate;

    auto listBeginOpt = proc->read<uintptr_t>(listPtr);
    auto listEndValueOpt = proc->read<uintptr_t>(listPtr + 0x8);

    Gui::log("[Bomb] planted list ptr=0x%llX listBegin=0x%llX listEndValue=0x%llX", (unsigned long long)listPtr,
              (unsigned long long)(listBeginOpt ? *listBeginOpt : 0ULL),
              (unsigned long long)(listEndValueOpt ? *listEndValueOpt : 0ULL));

    // Candidate 1: pointer itself may be planted C4 entity
    {
        auto cand = evaluateCandidate(listPtr);
        if (cand && cand->active) {
            info = *cand;
            goto finalize;
        }
        if (cand && !bestCandidate.has_value()) bestCandidate = *cand;
    }

    // Candidate 2: first pointer extracted from dwPlantedC4 structure
    if (listBeginOpt && *listBeginOpt != 0) {
        auto cand = evaluateCandidate(*listBeginOpt);
        if (cand && cand->active) {
            info = *cand;
            goto finalize;
        }
        if (cand && !bestCandidate.has_value()) bestCandidate = *cand;
    }

    // no further list scanning, DW_PLANTEDC4 is not a begin/end vector structure

finalize:
    if (!info.valid && bestCandidate.has_value()) {
        info = *bestCandidate;
    }

    if (!info.valid) {
        Gui::log("[Bomb] no valid planted C4 list yet");
        return info;
    }

    // Convert to remaining countdown. m_flC4Blow may be either absolute game time or direct remaining timer.
    float remaining;
    if (info.blowTime > 0.0f && info.blowTime <= 180.0f) {
        // Already a valid remaining timer (CS2 behavior or fallback returns relative time).
        remaining = info.blowTime;
    } else {
        remaining = info.blowTime - gameTime;
        if (remaining < 0.0f) remaining = 0.0f;
    }
    info.blowTime = remaining;

    Gui::log("[Bomb] origin read @ x=%.2f y=%.2f z=%.2f", info.origin.x, info.origin.y, info.origin.z);
    Gui::log("[Bomb] m_flC4Blow rem=%.3f", info.blowTime);
    Gui::log("[Bomb] active=%d", info.active);

    Gui::log("[Bomb] dwPlantedC4 @ 0x%llX points to planted C4 list 0x%llX",
              (unsigned long long)addr,
              (unsigned long long)info.plantedC4ClassPointer);

    return info;
}

} // namespace Bomb
