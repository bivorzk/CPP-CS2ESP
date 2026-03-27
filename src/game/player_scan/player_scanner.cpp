#include "player_scanner.hpp"
#include "player_scanner_helpers.hpp"
#include "gui.hpp"
#include "gui_tabs.hpp"
#include "overlay.hpp"
#include "bomb_found.hpp"

#include <array>
#include <algorithm>
#include <cmath>

namespace PlayerScanner {

static uint64_t s_scanFrameCount = 0;
static uint64_t s_scanLastFpsMs = 0;
static uint64_t s_scanTotalMs = 0;
static bool     s_scanDebug = false; // toggle for console profiling
#define DBG_LOG(...) do { if (s_scanDebug) Gui::log(__VA_ARGS__); } while(0)

bool scanPlayers(mem::ProcessMemory* proc, const Bomb::Info* bombInfo) {
    static std::vector<PlayerRecord> cachedPlayers;

    if (!proc) {
        Gui::log("[-] scanPlayers: invalid process pointer");
        return false;
    }

    uintptr_t base = proc->getModuleBase(Offsets::MODULE);
    if (base == 0) {
        Gui::log("[-] scanPlayers: module base not found: %s", Offsets::MODULE);
        return false;
    }

    auto entityListOpt = proc->read<uintptr_t>(base + Offsets::dwEntityList::STATIC_PTR);
    if (!entityListOpt || *entityListOpt == 0) {
        Gui::log("[-] scanPlayers: entity list pointer invalid (offset 0x%llX)", (unsigned long long)Offsets::dwEntityList::STATIC_PTR);
        return false;
    }

    auto localPlayerControllerOpt = proc->read<uintptr_t>(base + Offsets::dwLocalPlayerController::STATIC_PTR);
    auto localPlayerPawnOpt = proc->read<uintptr_t>(base + Offsets::dwLocalPlayerPawn::STATIC_PTR);
    uintptr_t localPlayerController = localPlayerControllerOpt.value_or(0);
    uintptr_t localPlayerPawn = localPlayerPawnOpt.value_or(0);

    int localTeam = -1;
    if (localPlayerPawn) {
        auto localTeamOpt = proc->read<int32_t>(localPlayerPawn + Offsets::m_iTeamNum::STATIC_PTR);
        if (localTeamOpt) localTeam = *localTeamOpt;
    }

    if (!localPlayerControllerOpt) {
        Gui::log("[-] scanPlayers: local player controller pointer invalid (offset 0x%llX)", (unsigned long long)Offsets::dwLocalPlayerController::STATIC_PTR);
    }
    if (!localPlayerPawn) {
        Gui::log("[-] scanPlayers: local player pawn pointer invalid (offset 0x%llX)", (unsigned long long)Offsets::dwLocalPlayerPawn::STATIC_PTR);
    }

    uintptr_t entityList = *entityListOpt;
    auto listEntryOpt = proc->read<uintptr_t>(entityList + Offsets::EntityList::ENTRY_OFFSET);
    if (!listEntryOpt || *listEntryOpt == 0) {
        Gui::log("[-] scanPlayers: first entity list entry invalid");
        return false;
    }
    uintptr_t listEntry = *listEntryOpt;

    const char* vmModule = "";
    uintptr_t vmBase = 0;
    for (const char* mod : { Offsets::MODULE, "engine2.dll", "engine.dll" }) {
        vmBase = proc->getModuleBase(mod);
        if (vmBase) { vmModule = mod; break; }
    }

    if (!vmBase) {
        DBG_LOG("[DBG] viewMatrix base module not found (client/engine2/engine)");
    }

    auto vmOpt = vmBase ? proc->read<std::array<float,16>>(vmBase + Offsets::dwViewMatrix::STATIC_PTR) : std::optional<std::array<float,16>>{};
    bool haveMatrix = vmOpt.has_value();
    std::array<float,16> viewMatrix{};
    if (haveMatrix) {
        viewMatrix = *vmOpt;
    }

    static std::optional<std::array<float,16>> prevViewMatrix;
    static std::vector<Overlay::PawnRenderInfo> prevPawnRects;

    std::vector<PlayerRecord> currentPlayers;
    std::vector<Overlay::PawnRenderInfo> pawnRects;
    Gui::VisualConfig visuals = Gui::getVisuals();
    bool showTeamABoxes = visuals.showTeamABoxes;

    RECT gameRect;
    if (!getCS2WindowRect(gameRect)) {
        DBG_LOG("[DBG] CS2 window not found, skipping scan");
        return false;
    }
    int gameW = gameRect.right - gameRect.left;
    int gameH = gameRect.bottom - gameRect.top;

    // Bomb direction filtering: only show friendly boxes when local reticle is roughly near bomb.
    bool localLookingAtBomb = false;
    Bomb::Info localBombInfo = bombInfo ? *bombInfo : Bomb::Finder::read(proc);
    const Bomb::Info* effectiveBombInfo = bombInfo ? bombInfo : &localBombInfo;
    if (haveMatrix && effectiveBombInfo->valid && effectiveBombInfo->active) {
        Vec3 bombWorld{effectiveBombInfo->origin.x, effectiveBombInfo->origin.y, effectiveBombInfo->origin.z};
        POINT bombPt;
        if (worldToScreen(bombWorld, viewMatrix, gameW, gameH, bombPt)) {
            int centerX = gameW / 2;
            int centerY = gameH / 2;
            int dx = abs(bombPt.x - centerX);
            int dy = abs(bombPt.y - centerY);
            int threshold = (std::min)(gameW, gameH) / 4;
            localLookingAtBomb = (dx <= threshold && dy <= threshold);
            if (s_scanDebug)
                DBG_LOG("[DBG] bomb screen delta dx=%d dy=%d threshold=%d localLookingAtBomb=%d", dx, dy, threshold, (int)localLookingAtBomb);
        }
    }

    for (int i = 1; i < 64; ++i) {
        auto currentControllerOpt = proc->read<uintptr_t>(listEntry + Offsets::EntityList::CHUNK_STRIDE * i);
        if (!currentControllerOpt || *currentControllerOpt == 0)
            continue;
        uintptr_t currentController = *currentControllerOpt;

        // do NOT skip local player controller; we need local player in Team A list (no ESP box)

        // We'll use pawn health for alive status, not controller boolean (may be stale for local)
        auto pawnHandleOpt = proc->read<uint32_t>(currentController + Offsets::m_hPlayerPawn::STATIC_PTR);
        if (!pawnHandleOpt || *pawnHandleOpt == 0 || *pawnHandleOpt == 0xFFFFFFFF)
            continue;
        uint32_t pawnHandle = *pawnHandleOpt;

        int pawnIndex = pawnHandle & 0x7FFF;
        int listIdx   = pawnIndex >> 9;
        int innerIdx  = pawnIndex & 0x1FF;

        auto listEntry2Opt = proc->read<uintptr_t>(entityList + 0x8 * listIdx + Offsets::EntityList::ENTRY_OFFSET);
        if (!listEntry2Opt || *listEntry2Opt == 0)
            continue;
        uintptr_t listEntry2 = *listEntry2Opt;

        auto pawnOpt = proc->read<uintptr_t>(listEntry2 + Offsets::EntityList::CHUNK_STRIDE * innerIdx);
        if (!pawnOpt || *pawnOpt == 0)
            continue;
        uintptr_t pawn = *pawnOpt;

        bool isSelfPawn = (localPlayerPawn && pawn == localPlayerPawn);

        // Determine team for this player (for UI team assignment and rectangle filtering)
        int gameTeam = -1;
        auto teamOpt = proc->read<int32_t>(currentController + Offsets::m_iTeamNum::STATIC_PTR);
        if (teamOpt) gameTeam = *teamOpt;

        int uiTeam = 1; // default Team B (enemies)
        bool drawBox = true;

        if (isSelfPawn) {
            uiTeam = 0;    // local player on Team A list
            drawBox = false;
        } else if (localTeam >= 0 && gameTeam == localTeam) {
            uiTeam = 0;    // local team mates on Team A list
            // Do not show team boxes for local team by default; world marker only if user wants.
            drawBox = false;
        } else {
            // enemies are shown (if user still wants Team A drawing) as a separate feature.
            drawBox = true;
        }

        auto healthOpt = proc->read<uint32_t>(pawn + Offsets::m_iHealth::STATIC_PTR);
        if (!healthOpt) {
            DBG_LOG("[DBG] pawn %llX health read failed", (unsigned long long)pawn);
            continue;
        }
        uint32_t health = *healthOpt;
        if (health == 0 || health > 100) {
            // ensure local player not falsely dead by defaulting to controller pawn health
            auto pawnHealthOpt = proc->read<uint32_t>(currentController + Offsets::m_iPawnHealth::STATIC_PTR);
            if (pawnHealthOpt && *pawnHealthOpt > 0 && *pawnHealthOpt <= 100) {
                health = *pawnHealthOpt;
            } else {
                continue;
            }
        }

        std::string name = readRemoteString(proc, currentController + Offsets::m_iszPlayerName::STATIC_PTR, 64);
        if (name.empty())
            continue;

        if (haveMatrix) {
            auto originOpt = proc->read<Vec3>(pawn + Offsets::m_vecOrigin::STATIC_PTR);
            if (!originOpt) {
                originOpt = proc->read<Vec3>(pawn + Offsets::m_vecOrigin::ABS_ORIGIN); // fallback
            }

            if (originOpt) {
                auto viewOffsetOpt = proc->read<Vec3>(pawn + Offsets::m_vecViewOffset::STATIC_PTR);

                Vec3 origin = *originOpt;

                // filter out bogus origin (0,0,0) which often maps to screen center
                if (fabs(origin.x) < 1.0f && fabs(origin.y) < 1.0f && fabs(origin.z) < 1.0f) {
                    // don't add stale center-case entity
                } else {
                    Vec3 head = origin;
                    if (viewOffsetOpt) {
                        head.x += viewOffsetOpt->x;
                        head.y += viewOffsetOpt->y;
                        head.z += viewOffsetOpt->z;
                    }

                    POINT footPt, headPt;
                    bool footOk = worldToScreen(origin, viewMatrix, gameW, gameH, footPt);
                    bool headOk = worldToScreen(head, viewMatrix, gameW, gameH, headPt);

                    if (footOk && headOk) {
                        int left = std::min(footPt.x, headPt.x) - 12;
                        int right = std::max(footPt.x, headPt.x) + 12;
                        int fullHeight = std::max(footPt.y, headPt.y) - std::min(footPt.y, headPt.y);
                        int shrinkAmount = (int)(fullHeight * 0.3f); // shave 30% from total height
                        int top = std::min(footPt.y, headPt.y) - 24 + shrinkAmount/2;
                        int bottom = std::max(footPt.y, headPt.y) + 12 - shrinkAmount/2;
                        RECT targetRect = {left, top, right, bottom};

                        Overlay::PawnRenderInfo info;
                        info.rect = targetRect;
                        strncpy_s(info.name, name.c_str(), _TRUNCATE);
                        info.health = health;
                        info.drawBox = drawBox;
                        info.teamA = (uiTeam == 0);
                        info.isBomb = false;
                        pawnRects.push_back(info);
                    } else {
                        DBG_LOG("[DBG] w2s failed for pawn (footOk=%d headOk=%d)", footOk, headOk);
                    }
                }
            } else {
                DBG_LOG("[DBG] origin read failed for pawn %llX", (unsigned long long)pawn);
            }
        }

        currentPlayers.push_back(PlayerRecord{i, uiTeam, std::move(name), health});
    }

    bool listChanged = (currentPlayers != cachedPlayers);
    cachedPlayers = currentPlayers;

    // Add bomb marker to overlay if bomb is planted / actively ticking (not dropped, not a half-read coin).
    static std::optional<Vec3> g_lastBombWorld;
    const Bomb::Info* bombInfoPtr = effectiveBombInfo;

    if (!bombInfoPtr->active) {
        g_lastBombWorld.reset();
    }

    if (!bombInfoPtr->active || !bombInfoPtr->valid || !haveMatrix) {
        // no active planted bomb to show
    } else {
        POINT bombPt;
        Vec3 bombWorld { bombInfoPtr->origin.x, bombInfoPtr->origin.y, bombInfoPtr->origin.z };

        auto isBogusBombOrigin = [&](const Vec3 &v) {
            return fabs(v.x) < 1.0f && fabs(v.y) < 1.0f && fabs(v.z) < 1.0f;
        };

        bool canDrawBomb = true;
        if (isBogusBombOrigin(bombWorld)) {
            if (g_lastBombWorld.has_value()) {
                bombWorld = *g_lastBombWorld;
                Gui::log("[Bomb] active planted bomb origin bogus; using last known position %.2f,%.2f,%.2f", bombWorld.x, bombWorld.y, bombWorld.z);
            } else {
                Gui::log("[Bomb] active planted bomb has bogus origin and no last known position yet; skipping display");
                canDrawBomb = false;
            }
        }

        bool bombOk = false;
        if (canDrawBomb) bombOk = worldToScreen(bombWorld, viewMatrix, gameW, gameH, bombPt);
        if (!bombOk) {
            static const float boostZ[] = { 8.0f, 16.0f, 32.0f, 64.0f };
            for (float dz : boostZ) {
                Vec3 tmp = bombWorld;
                tmp.z += dz;
                if (worldToScreen(tmp, viewMatrix, gameW, gameH, bombPt)) {
                    bombOk = true;
                    bombWorld = tmp;
                    Gui::log("[Bomb] w2s fallback z+%.1f succeeded at %d,%d", dz, bombPt.x, bombPt.y);
                    break;
                }
            }
        }

        if (!bombOk && g_lastBombWorld.has_value()) {
            Vec3 tmp = *g_lastBombWorld;
            if (worldToScreen(tmp, viewMatrix, gameW, gameH, bombPt)) {
                bombOk = true;
                bombWorld = tmp;
                Gui::log("[Bomb] w2s lastBombWorld fallback succeeded at %d,%d", bombPt.x, bombPt.y);
            }
        }

        if (bombOk) {
            g_lastBombWorld = bombWorld;
            Overlay::PawnRenderInfo bombRect{};
            bombRect.rect.left = bombPt.x - 16;
            bombRect.rect.right = bombPt.x + 16;
            // shift center down to better match bomb on ground
            bombRect.rect.top = bombPt.y - 24 + 8;
            bombRect.rect.bottom = bombPt.y + 24 + 8;
            strncpy_s(bombRect.name, "BOMB", _TRUNCATE);
            bombRect.health = 0;
            bombRect.drawBox = true;
            bombRect.teamA = false;
            bombRect.isBomb = true;
            bombRect.blowTime = bombInfoPtr->blowTime;
            pawnRects.push_back(bombRect);

            Gui::log("[Bomb] worldToScreen succeeded at %d,%d", bombPt.x, bombPt.y);
        } else {
            Gui::log("[Bomb] worldToScreen failed for bomb origin (raw: %.2f,%.2f,%.2f)",
                    bombInfoPtr->origin.x, bombInfoPtr->origin.y, bombInfoPtr->origin.z);
        }
    }

    // Real-time mode: update overlays every tick, no gating.
    // This can be expensive but gives immediate player/bomb tracking during camera/player motion.
    prevPawnRects = pawnRects;
    Overlay::setPawnRects(pawnRects);

    Gui::clearPlayers();

    if (currentPlayers.empty()) {
        return true;
    }

    if (getCS2WindowRect(gameRect)) {
        int gameW = gameRect.right - gameRect.left;
        int gameH = gameRect.bottom - gameRect.top;
        Overlay::setOverlayBounds(gameRect.left, gameRect.top, gameW, gameH);
    }

    for (auto& player : currentPlayers) {
        int slot = (player.index - 1) % 5;
        int team = player.team;
        if (team < 0 || team > 1) team = 1;

        Gui::updatePlayer(team, slot, player.name.c_str(), (int)player.health, 100, true);
    }

    return true;
}

} // namespace PlayerScanner
