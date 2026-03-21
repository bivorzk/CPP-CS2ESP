#include "player_scanner.hpp"
#include "gui.hpp"
#include "gui_tabs.hpp"
#include "overlay.hpp"

#include <array>
#include <algorithm>
#include <cmath>

namespace Gui {
    void clearPlayers();
    void updatePlayer(int team, int slot, const char* name, int hp, int hpMax, bool alive);
}

struct Vec3 { float x, y, z; };

bool worldToScreen(const Vec3& world,
                   const std::array<float, 16>& m,
                   int width, int height,
                   POINT& out)
{
    auto project = [&](const std::array<float,16>& mat, float &clipX, float &clipY, float &clipW){
        clipX = world.x * mat[0] + world.y * mat[1] + world.z * mat[2] + mat[3];
        clipY = world.x * mat[4] + world.y * mat[5] + world.z * mat[6] + mat[7];
        clipW = world.x * mat[12] + world.y * mat[13] + world.z * mat[14] + mat[15];
    };

    float clipX, clipY, clipW;
    project(m, clipX, clipY, clipW);
    if (clipW < 0.0001f) {
        // try transposed row-major style (fallback)
        clipX = world.x * m[0] + world.y * m[4] + world.z * m[8]  + m[12];
        clipY = world.x * m[1] + world.y * m[5] + world.z * m[9]  + m[13];
        clipW = world.x * m[3] + world.y * m[7] + world.z * m[11] + m[15];
    }

    if (clipW < 0.0001f) return false;

    float ndcX = clipX / clipW;
    float ndcY = clipY / clipW;

    if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f) return false;

    out.x = (int)((width / 2.0f) + (ndcX * width / 2.0f));
    out.y = (int)((height / 2.0f) - (ndcY * height / 2.0f));
    return true;
}

static bool getCS2WindowRect(RECT& out) {
    HWND hCS2 = FindWindowA("Valve001", nullptr);
    if (!hCS2) hCS2 = FindWindowA(nullptr, "Counter-Strike 2");
    if (!hCS2) return false;
    GetWindowRect(hCS2, &out);
    return true;
}

namespace PlayerScanner {

std::string readRemoteString(mem::ProcessMemory* proc,
                             uintptr_t address,
                             size_t maxLen)
{
    if (!proc || address == 0 || maxLen == 0) return "";

    std::array<char, 128> buffer{};
    size_t toRead = (maxLen < buffer.size() - 1 ? maxLen : buffer.size() - 1);
    if (!proc->readBytes(address, buffer.data(), toRead))
        return "";

    buffer[toRead] = '\0';
    return std::string(buffer.data());
}

struct PlayerRecord {
    int index;
    int team;            // 0=local team (Team A), 1=enemy team (Team B)
    std::string name;
    uint32_t health;

    bool operator==(const PlayerRecord& o) const noexcept {
        return index == o.index && team == o.team && health == o.health && name == o.name;
    }
};

bool scanPlayers(mem::ProcessMemory* proc) {
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
        Gui::log("[DBG] viewMatrix base module not found (client/engine2/engine)");
    }

    auto vmOpt = vmBase ? proc->read<std::array<float,16>>(vmBase + Offsets::dwViewMatrix::STATIC_PTR) : std::optional<std::array<float,16>>{};
    bool haveMatrix = vmOpt.has_value();
    std::array<float,16> viewMatrix{};
    if (haveMatrix) {
        viewMatrix = *vmOpt;
        Gui::log("[DBG] view matrix from %s (base=0x%llX)", vmModule ? vmModule : "unknown", (unsigned long long)vmBase);
        Gui::log("[DBG] viewMatrix[0..3]=%.6f %.6f %.6f %.6f", viewMatrix[0], viewMatrix[1], viewMatrix[2], viewMatrix[3]);
        Gui::log("[DBG] viewMatrix[12..15]=%.6f %.6f %.6f %.6f", viewMatrix[12], viewMatrix[13], viewMatrix[14], viewMatrix[15]);
    } else {
        Gui::log("[DBG] viewMatrix read failed, module=%s base=0x%llX offset=0x%llX", vmModule ? vmModule : "unknown", (unsigned long long)vmBase, (unsigned long long)Offsets::dwViewMatrix::STATIC_PTR);
    }

    std::vector<PlayerRecord> currentPlayers;
    std::vector<RECT> pawnRects;
    Gui::VisualConfig visuals = Gui::getVisuals();
    bool showTeamABoxes = visuals.showTeamABoxes;

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
            drawBox = showTeamABoxes;
        }

        auto healthOpt = proc->read<uint32_t>(pawn + Offsets::m_iHealth::STATIC_PTR);
        if (!healthOpt) {
            Gui::log("[DBG] pawn %llX health read failed", (unsigned long long)pawn);
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
            auto viewOffsetOpt = proc->read<Vec3>(pawn + Offsets::m_vecViewOffset::STATIC_PTR);

            if (originOpt) {
                Vec3 origin = *originOpt;

                // filter out bogus origin (0,0,0) which often maps to screen center
                if (fabs(origin.x) < 1.0f && fabs(origin.y) < 1.0f && fabs(origin.z) < 1.0f) {
                    Gui::log("[DBG] skipping pawn %llX near-zero origin", (unsigned long long)pawn);
                    continue;
                }

                Vec3 head = origin;
                if (viewOffsetOpt) {
                    head.x += viewOffsetOpt->x;
                    head.y += viewOffsetOpt->y;
                    head.z += viewOffsetOpt->z;
                }

                RECT gameRect;
                if (getCS2WindowRect(gameRect)) {
                    int gameW = gameRect.right - gameRect.left;
                    int gameH = gameRect.bottom - gameRect.top;
                    POINT footPt, headPt;
                    bool footOk = worldToScreen(*originOpt, viewMatrix, gameW, gameH, footPt);
                    bool headOk = worldToScreen(head, viewMatrix, gameW, gameH, headPt);

                    Gui::log("[DBG] pawn=%llX origin=%.2f,%.2f,%.2f head=%.2f,%.2f,%.2f", (unsigned long long)pawn,
                             originOpt->x, originOpt->y, originOpt->z,
                             head.x, head.y, head.z);

                    if (footOk && headOk) {
                        if (drawBox) {
                            int left = std::min(footPt.x, headPt.x) - 12;
                            int right = std::max(footPt.x, headPt.x) + 12;
                            int top = std::min(footPt.y, headPt.y) - 24;
                            int bottom = std::max(footPt.y, headPt.y) + 12;
                            RECT targetRect = {left, top, right, bottom};
                            pawnRects.push_back(targetRect);
                        }
                    } else {
                        Gui::log("[DBG] w2s failed for pawn (footOk=%d headOk=%d)", footOk, headOk);
                    }
                }
            } else {
                Gui::log("[DBG] origin read failed for pawn %llX", (unsigned long long)pawn);
            }
        }

        currentPlayers.push_back({i, uiTeam, std::move(name), health});
    }

    bool listChanged = (currentPlayers != cachedPlayers);
    cachedPlayers = currentPlayers;

    Gui::clearPlayers();
    Overlay::setPawnRects(pawnRects);

    if (currentPlayers.empty()) {
        Gui::log("[-] scanPlayers: no valid players found (verify offsets and entity structure)");
        return true;
    }

    Gui::log("[Scan] entityList: 0x%llX firstEntry: 0x%llX count=%zu",
              (unsigned long long)entityList,
              (unsigned long long)listEntry,
              currentPlayers.size());

    RECT gameRect;
    if (getCS2WindowRect(gameRect)) {
        int gameW = gameRect.right - gameRect.left;
        int gameH = gameRect.bottom - gameRect.top;
        Overlay::setOverlayBounds(gameRect.left, gameRect.top, gameW, gameH);
    }

    for (auto& player : currentPlayers) {
        Gui::log("[Player] idx=%d team=%d name=%s hp=%u", player.index, player.team, player.name.c_str(), player.health);

        int slot = (player.index - 1) % 5;
        int team = player.team;
        if (team < 0 || team > 1) team = 1;

        Gui::updatePlayer(team, slot, player.name.c_str(), (int)player.health, 100, true);
    }

    return true;
}

} // namespace PlayerScanner
