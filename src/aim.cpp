#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "aim.hpp"
#include "offsets.hpp"
#include "gui.hpp"
#include <array>
#include <cmath>
#include <string>
#include <windows.h>
#include <psapi.h>

static bool isCs2ForegroundWindow() {
    HWND foreground = GetForegroundWindow();
    if (!foreground) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    if (pid == 0) return false;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return false;

    char path[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    bool ok = QueryFullProcessImageNameA(hProcess, 0, path, &size) != 0;
    CloseHandle(hProcess);
    if (!ok) return false;

    std::string lowerPath(path);
    for (char &c : lowerPath) c = static_cast<char>(tolower(c));
    if (lowerPath.find("cs2.exe") != std::string::npos) return true;
    if (lowerPath.find("counter-strike 2.exe") != std::string::npos) return true;
    return false;
}

struct Vec3 { float x, y, z; };

static float clampf01(float v, float minv, float maxv) {
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

static float normalizeYaw(float yaw) {
    while (yaw > 180.0f) yaw -= 360.0f;
    while (yaw < -180.0f) yaw += 360.0f;
    return yaw;
}

static Vec3 calcAngle(const Vec3& src, const Vec3& dst) {
    Vec3 delta{dst.x - src.x, dst.y - src.y, dst.z - src.z};
    float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);

    Vec3 ang;
    ang.x = -atan2f(delta.z, hyp) * (180.0f / 3.14159265358979323846f);
    ang.y = atan2f(delta.y, delta.x) * (180.0f / 3.14159265358979323846f);
    ang.z = 0.0f;

    ang.x = clampf01(ang.x, -89.0f, 89.0f);
    ang.y = normalizeYaw(ang.y);

    return ang;
}

static float distance3d(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

static bool isPawnVisible(mem::ProcessMemory* proc, uintptr_t pawn, int localPlayerId, bool strictMaskOnly = false) {
    if (!proc || !pawn) return false;

    // treat dormant as not visible
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
    bool localIdValid = (localPlayerId >= 0 && localPlayerId < 32);
    if (hasMask && localIdValid) {
        spottedByLocal = (((*maskOpt) >> localPlayerId) & 1u) != 0;
    }

    bool visible = false;
    if (strictMaskOnly) {
        // strict condition: we only trust the local mask bit; avoids stale spotted state cases.
        if (hasMask && localIdValid) {
            visible = spottedByLocal;
            if (!visible) Gui::log("[DBG] pawn=0x%llX strict-visibility=fail mask(local) mask=0x%X local=%d", (unsigned long long)pawn, *maskOpt, localPlayerId);
        } else {
            visible = false;
            Gui::log("[DBG] pawn=0x%llX strict-visibility=fail mask/missing-local", (unsigned long long)pawn);
        }
    } else {
        if (hasMask && localIdValid) {
            visible = spottedByLocal;
            if (!visible) Gui::log("[DBG] pawn=0x%llX visibility=fail mask(local) mask=0x%X local=%d", (unsigned long long)pawn, *maskOpt, localPlayerId);
        } else if (hasSpotted) {
            visible = spotted;
            if (!visible) Gui::log("[DBG] pawn=0x%llX visibility=fail spotted=%d", (unsigned long long)pawn, spotted ? 1 : 0);
        } else if (hasMask) {
            visible = false;
            Gui::log("[DBG] pawn=0x%llX visibility=fail mask-only local invalid=%d", (unsigned long long)pawn, localIdValid ? 0 : 1);
        }
    }

    Gui::log("[DBG] pawn=0x%llX dormant=%d spotted=%d mask=0x%X local=%d visible=%d", (unsigned long long)pawn, dormant ? 1 : 0,
             hasSpotted ? (spotted ? 1 : 0) : -1,
             hasMask ? *maskOpt : 0u,
             localPlayerId,
             visible ? 1 : 0);

    return visible;
}

static bool worldToScreen(const Vec3& world,
                          const std::array<float,16>& m,
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

static int getLocalPlayerId(mem::ProcessMemory* proc, uintptr_t entityList, uintptr_t localPawn) {
    if (!proc || entityList == 0 || localPawn == 0) return -1;

    auto firstEntityOpt = proc->read<uintptr_t>(entityList + Offsets::EntityList::ENTRY_OFFSET);
    if (!firstEntityOpt || *firstEntityOpt == 0) return -1;
    uintptr_t firstEntity = *firstEntityOpt;

    for (int i = 0; i < 64; ++i) {
        auto ctrlOpt = proc->read<uintptr_t>(firstEntity + Offsets::EntityList::CHUNK_STRIDE * i);
        if (!ctrlOpt || *ctrlOpt == 0) continue;
        uintptr_t ctrl = *ctrlOpt;

        auto pawnHandleOpt = proc->read<uint32_t>(ctrl + Offsets::m_hPlayerPawn::STATIC_PTR);
        if (!pawnHandleOpt || *pawnHandleOpt == 0 || *pawnHandleOpt == 0xFFFFFFFF) continue;

        uint32_t pawnHandle = *pawnHandleOpt;
        uint32_t pawnIdx = pawnHandle & 0x7FFF;
        uint32_t listIdx = pawnIdx >> 9;
        uint32_t innerIdx = pawnIdx & 0x1FF;

        auto chunk2Opt = proc->read<uintptr_t>(entityList + 0x8 * listIdx + Offsets::EntityList::ENTRY_OFFSET);
        if (!chunk2Opt || *chunk2Opt == 0) continue;
        auto pawnOpt = proc->read<uintptr_t>(*chunk2Opt + Offsets::EntityList::CHUNK_STRIDE * innerIdx);
        if (!pawnOpt || *pawnOpt == 0) continue;

        if (*pawnOpt == localPawn) {
            return i;
        }
    }
    return -1;
}

static float angdiff(float a, float b) {
    float d = a - b;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

void Aim::update(mem::ProcessMemory* proc) {
    if (!proc) return;

    bool cs2Foreground = isCs2ForegroundWindow();
    bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool autoAim = Gui::getVisuals().autoAim;
    if (!cs2Foreground) return;
    if (!altDown && !autoAim) return;

    bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool altFire = (altDown && leftDown);
    bool autoFire = autoAim || leftDown || altFire;

    static bool prevLeftDown = false;
    bool leftJustPressed = leftDown && !prevLeftDown;
    bool leftJustReleased = !leftDown && prevLeftDown;
    prevLeftDown = leftDown;

    // ALT+LMB is rapid-fire mode (hold down behavior). Normal click is one tap.
    uint64_t rapidDelayMs = 0;
    uint64_t normalDelayMs = 40;

    struct RcsConfig { bool enabled; float strength, smooth, sensitivity; };
    RcsConfig rcs{false, 0.6f, 0.35f, 1.8f};
    // TODO: implement Gui::getRCS() and use it here (enabled/strength/smooth).
    // auto rcsCfg = Gui::getRCS(); rcs.enabled = rcsCfg.enabled; etc.

    static const std::array<std::pair<float,float>, 31> akPattern {{
        {0.00f, 1.20f}, {0.12f, 1.35f}, {-0.08f, 1.55f}, {0.10f, 1.72f}, {-0.15f, 1.90f},
        {0.20f, 2.10f}, {-0.18f, 2.30f}, {0.22f, 2.52f}, {-0.30f, 2.75f}, {0.35f, 2.98f},
        {0.40f, 3.25f}, {-0.40f, 3.65f}, {0.55f, 4.00f}, {-0.55f, 4.45f}, {0.70f, 4.95f},
        {0.70f, 5.50f}, {-0.80f, 6.10f}, {0.90f, 6.80f}, {-1.05f, 7.65f}, {1.20f, 8.55f},
        {-1.45f, 9.60f}, {1.70f, 10.70f}, {-1.90f, 11.90f}, {2.15f, 13.25f}, {-2.45f, 14.75f},
        {2.85f, 16.40f}, {-3.25f, 18.20f}, {3.80f, 20.15f}, {-4.45f, 22.30f}, {5.30f, 24.75f},
        {0.00f, 0.00f}
    }};

    static const std::array<std::pair<float,float>, 31> m4Pattern {{
        {0.00f, 1.00f}, {0.10f, 1.14f}, {-0.06f, 1.30f}, {0.08f, 1.42f}, {-0.12f, 1.58f},
        {0.16f, 1.72f}, {-0.14f, 1.88f}, {0.18f, 2.02f}, {-0.25f, 2.20f}, {0.30f, 2.40f},
        {0.36f, 2.62f}, {-0.37f, 2.90f}, {0.45f, 3.20f}, {-0.50f, 3.54f}, {0.60f, 3.95f},
        {0.65f, 4.35f}, {-0.75f, 4.80f}, {0.85f, 5.35f}, {-0.95f, 5.95f}, {1.05f, 6.65f},
        {-1.25f, 7.45f}, {1.45f, 8.35f}, {-1.70f, 9.35f}, {1.95f, 10.45f}, {-2.25f, 11.70f},
        {2.60f, 13.10f}, {-3.00f, 14.70f}, {3.45f, 16.50f}, {-4.00f, 18.50f}, {4.60f, 20.75f},
        {0.00f, 0.00f}
    }};

    static uintptr_t g_targetPawn = 0;
    static uintptr_t g_pendingPawn = 0;
    static int      g_pendingFrames = 0;
    static int      g_lostFrames = 0;
    static Vec3 previousRcsDelta {0.0f, 0.0f, 0.0f};

    uintptr_t base = proc->getModuleBase(Offsets::MODULE);
    if (!base) return;

    auto localPawnOpt = proc->read<uintptr_t>(base + Offsets::dwLocalPlayerPawn::STATIC_PTR);
    if (!localPawnOpt || *localPawnOpt == 0) return;
    uintptr_t localPawn = *localPawnOpt;

    auto localTeamOpt = proc->read<int32_t>(localPawn + Offsets::m_iTeamNum::STATIC_PTR);
    auto localOriginOpt = proc->read<Vec3>(localPawn + Offsets::m_vecOrigin::STATIC_PTR);
    auto localViewOffsetOpt = proc->read<Vec3>(localPawn + Offsets::m_vecViewOffset::STATIC_PTR);
    if (!localTeamOpt || !localOriginOpt || !localViewOffsetOpt) return;

    int localTeam = *localTeamOpt;
    Vec3 eyeOrigin = *localOriginOpt;
    eyeOrigin.x += localViewOffsetOpt->x;
    eyeOrigin.y += localViewOffsetOpt->y;
    eyeOrigin.z += localViewOffsetOpt->z;

    auto entityListOpt = proc->read<uintptr_t>(base + Offsets::dwEntityList::STATIC_PTR);
    if (!entityListOpt || *entityListOpt == 0) return;
    uintptr_t entityList = *entityListOpt;

    auto firstEntityOpt = proc->read<uintptr_t>(entityList + Offsets::EntityList::ENTRY_OFFSET);
    if (!firstEntityOpt || *firstEntityOpt == 0) return;
    uintptr_t firstEntity = *firstEntityOpt;

    int localPlayerId = getLocalPlayerId(proc, entityList, localPawn);
    if (localPlayerId < 0) localPlayerId = -1;

    const auto& visCfg = Gui::getVisuals();
    const float maxRange = 1600.0f;
    const float maxFov = 60.0f;
    const float strictMaxRange = 1450.0f;
    const float strictMaxYaw = 65.0f;
    const float strictMaxPitch = 65.0f;
    Vec3 bestTargetHead {0.0f, 0.0f, 0.0f};
    float bestScore = 1e9f;
    bool found = false;
    uintptr_t chosenPawn = 0;

    auto validatePawn = [&](uintptr_t pawn)->bool {
        if (!pawn) return false;
        auto teamOpt = proc->read<int32_t>(pawn + Offsets::m_iTeamNum::STATIC_PTR);
        auto healthOpt = proc->read<int32_t>(pawn + Offsets::m_iHealth::STATIC_PTR);
        auto originOpt = proc->read<Vec3>(pawn + Offsets::m_vecOrigin::STATIC_PTR);
        auto viewOffsetOpt = proc->read<Vec3>(pawn + Offsets::m_vecViewOffset::STATIC_PTR);
        if (!teamOpt || !healthOpt || !originOpt || !viewOffsetOpt) return false;
        if (*healthOpt <= 0) return false;
        if (*teamOpt == localTeam) return false;

        auto dormantOpt = proc->read<uint8_t>(pawn + Offsets::m_bDormant::STATIC_PTR);
        auto spottedOpt = proc->read<uint8_t>(pawn + Offsets::m_bSpotted::STATIC_PTR);
        auto maskOpt = proc->read<uint32_t>(pawn + Offsets::m_bSpottedByMask::STATIC_PTR);
        bool visible = isPawnVisible(proc, pawn, localPlayerId);

        if (!visible) return false;

        Vec3 head = *originOpt;
        head.x += viewOffsetOpt->x;
        head.y += viewOffsetOpt->y;
        head.z += viewOffsetOpt->z;
        float dist = distance3d(eyeOrigin, head);
        return dist <= maxRange;
    };

    auto curAngOpt = proc->read<Vec3>(localPawn + Offsets::m_angEyeAngles::STATIC_PTR);
    if (!curAngOpt) return;
    Vec3 curAng = *curAngOpt;

    // Candidate target stickiness & delayed release (optional): set to 0 for immediate reaction.
    static uintptr_t candidatePawn = 0;
    static int candidateFrames = 0;
    static int lostFrames = 0;
    int framesRequired = std::max(0, Gui::getVisuals().visCooldownFrames);

    if (g_targetPawn && isPawnVisible(proc, g_targetPawn, localPlayerId, visCfg.strictVisibility)) {
        auto originOpt = proc->read<Vec3>(g_targetPawn + Offsets::m_vecOrigin::STATIC_PTR);
        auto viewOffsetOpt = proc->read<Vec3>(g_targetPawn + Offsets::m_vecViewOffset::STATIC_PTR);
        if (originOpt && viewOffsetOpt) {
            Vec3 head = *originOpt;
            head.x += viewOffsetOpt->x;
            head.y += viewOffsetOpt->y;
            head.z += viewOffsetOpt->z;
            bestTargetHead = head;
            found = true;
            chosenPawn = g_targetPawn;
            // keep existing target as initial preference while we scan
        }
    }

    for (int i = 1; i < 64; ++i) {
        auto ctrlOpt = proc->read<uintptr_t>(firstEntity + Offsets::EntityList::CHUNK_STRIDE * i);
        if (!ctrlOpt || *ctrlOpt == 0) continue;
        uintptr_t ctrl = *ctrlOpt;

        auto pawnHandleOpt = proc->read<uint32_t>(ctrl + Offsets::m_hPlayerPawn::STATIC_PTR);
        if (!pawnHandleOpt || *pawnHandleOpt == 0 || *pawnHandleOpt == 0xFFFFFFFF) continue;

        uint32_t pawnHandle = *pawnHandleOpt;
        uint32_t pawnIdx = pawnHandle & 0x7FFF;
        uint32_t listIdx = pawnIdx >> 9;
        uint32_t innerIdx = pawnIdx & 0x1FF;

        auto chunkOpt = proc->read<uintptr_t>(entityList + 0x8 * listIdx + Offsets::EntityList::ENTRY_OFFSET);
        if (!chunkOpt || *chunkOpt == 0) continue;
        auto pawnOpt = proc->read<uintptr_t>(*chunkOpt + Offsets::EntityList::CHUNK_STRIDE * innerIdx);
        if (!pawnOpt || *pawnOpt == 0) continue;
        uintptr_t pawn = *pawnOpt;

        if (pawn == localPawn) {
            // m_bSpottedByMask is indexed by pawn handle index; use pawnIdx
            localPlayerId = static_cast<int>(pawnIdx);
        }

        auto teamOpt = proc->read<int32_t>(pawn + Offsets::m_iTeamNum::STATIC_PTR);
        if (!teamOpt || *teamOpt == localTeam) continue;

        auto hpOpt = proc->read<int32_t>(pawn + Offsets::m_iHealth::STATIC_PTR);
        if (!hpOpt || *hpOpt <= 0) continue;

        if (!isPawnVisible(proc, pawn, localPlayerId, visCfg.strictVisibility)) continue;

        auto originOpt = proc->read<Vec3>(pawn + Offsets::m_vecOrigin::STATIC_PTR);
        auto viewOffsetOptEnemy = proc->read<Vec3>(pawn + Offsets::m_vecViewOffset::STATIC_PTR);
        if (!originOpt || !viewOffsetOptEnemy) continue;

        Vec3 head = *originOpt;
        head.x += viewOffsetOptEnemy->x;
        head.y += viewOffsetOptEnemy->y;
        head.z += viewOffsetOptEnemy->z;

        float dist = distance3d(eyeOrigin, head);
        if (dist > maxRange) continue;

        Vec3 aim = calcAngle(eyeOrigin, head);
        float yawDelta = fabsf(angdiff(aim.y, curAng.y));
        float pitchDelta = fabsf(angdiff(aim.x, curAng.x));
        float fov = sqrtf(yawDelta * yawDelta + pitchDelta * pitchDelta);
        if (fov > maxFov) continue;

        float score = fov + (dist / maxRange) * 2.0f;
        if (!found || score < bestScore) {
            found = true;
            bestScore = score;
            bestTargetHead = head;
            chosenPawn = pawn;
        }
    }

    // Apply candidate selection / stickiness
    if (found) {
        if (g_pendingPawn == chosenPawn) {
            g_pendingFrames += 1;
        } else {
            g_pendingPawn = chosenPawn;
            g_pendingFrames = 1;
        }
    } else {
        g_pendingPawn = 0;
        g_pendingFrames = 0;
    }

    int requiredFrames = std::max(0, visCfg.visCooldownFrames);
    bool confirmedCandidate = (g_pendingPawn != 0 && g_pendingFrames >= requiredFrames);

    if (confirmedCandidate) {
        if (g_targetPawn != g_pendingPawn) {
            Gui::log("[DBG] target confirmed after %d frames: 0x%llX", g_pendingFrames, (unsigned long long)g_pendingPawn);
        }
        g_targetPawn = g_pendingPawn;
        g_lostFrames = 0;
    } else {
        if (g_targetPawn != 0) {
            bool currentStillVisible = isPawnVisible(proc, g_targetPawn, localPlayerId, visCfg.strictVisibility);
            if (!currentStillVisible) {
                g_lostFrames += 1;
                Gui::log("[DBG] target lost-frame %d for 0x%llX", g_lostFrames, (unsigned long long)g_targetPawn);
                if (g_lostFrames > requiredFrames) {
                    Gui::log("[DBG] target dropped (visibility loss) 0x%llX", (unsigned long long)g_targetPawn);
                    g_targetPawn = 0;
                }
            } else {
                g_lostFrames = 0;
            }
        }
    }

    if (!g_targetPawn) {
        return;
    }

    if (g_targetPawn != chosenPawn && !found) {
        // keep previous target for short sticky period while no better target is confirmed
        auto originOpt = proc->read<Vec3>(g_targetPawn + Offsets::m_vecOrigin::STATIC_PTR);
        auto viewOffsetOpt = proc->read<Vec3>(g_targetPawn + Offsets::m_vecViewOffset::STATIC_PTR);
        if (originOpt && viewOffsetOpt) {
            bestTargetHead = *originOpt;
            bestTargetHead.x += viewOffsetOpt->x;
            bestTargetHead.y += viewOffsetOpt->y;
            bestTargetHead.z += viewOffsetOpt->z;
        } else {
            g_targetPawn = 0;
            return;
        }
    }

    // Strict visibility enlargement: require within distance and view cone (avoids delayed through-wall connections)
    if (visCfg.strictVisibility) {
        float dist = distance3d(eyeOrigin, bestTargetHead);
        if (dist > strictMaxRange) {
            Gui::log("[DBG] pawn=0x%llX strict fail dist=%.1f > %.1f", (unsigned long long)g_targetPawn, dist, strictMaxRange);
            return;
        }
        Vec3 aim = calcAngle(eyeOrigin, bestTargetHead);
        float yawDelta = fabsf(angdiff(aim.y, curAng.y));
        float pitchDelta = fabsf(angdiff(aim.x, curAng.x));
        if (yawDelta > strictMaxYaw || pitchDelta > strictMaxPitch) {
            Gui::log("[DBG] pawn=0x%llX strict fail cone yaw=%.1f pitch=%.1f", (unsigned long long)g_targetPawn, yawDelta, pitchDelta);
            return;
        }
    }

    Vec3 desiredAng = calcAngle(eyeOrigin, bestTargetHead);
    proc->write<Vec3>(localPawn + Offsets::m_angEyeAngles::STATIC_PTR, desiredAng);

    auto vmOpt = proc->read<std::array<float,16>>(base + Offsets::dwViewMatrix::STATIC_PTR);
    RECT rc;
    if (vmOpt && getCS2WindowRect(rc)) {
        POINT headPoint;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (worldToScreen(bestTargetHead, *vmOpt, w, h, headPoint)) {
            int centerX = w / 2;
            int centerY = h / 2;
            int dx = headPoint.x - centerX;
            int dy = headPoint.y - centerY;
            INPUT mi{};
            mi.type = INPUT_MOUSE;
            mi.mi.dx = dx;
            mi.mi.dy = dy;
            mi.mi.dwFlags = MOUSEEVENTF_MOVE;
            SendInput(1, &mi, sizeof(INPUT));
        }
    }

    // Recoil control system
    if (rcs.enabled && autoFire) {
        Vec3 aimPunch{0.0f, 0.0f, 0.0f};
        auto punchOpt = proc->read<Vec3>(localPawn + Offsets::m_aimPunchAngle::STATIC_PTR);
        if (punchOpt) aimPunch = *punchOpt;

        uintptr_t weaponPawn = 0;
        auto wsOpt = proc->read<uintptr_t>(localPawn + Offsets::m_pWeaponServices::STATIC_PTR);
        if (wsOpt && *wsOpt) {
            auto weaponHandleOpt = proc->read<uint32_t>(*wsOpt + Offsets::m_hActiveWeapon::STATIC_PTR);
            if (weaponHandleOpt && *weaponHandleOpt != 0 && *weaponHandleOpt != 0xFFFFFFFF) {
                uint32_t weaponHandle = *weaponHandleOpt;
                uint32_t weaponIdx = weaponHandle & 0x7FFF;
                uint32_t listIdx = weaponIdx >> 9;
                uint32_t innerIdx = weaponIdx & 0x1FF;
                auto weaponBlockOpt = proc->read<uintptr_t>(entityList + 0x8 * listIdx + Offsets::EntityList::ENTRY_OFFSET);
                if (weaponBlockOpt && *weaponBlockOpt) {
                    auto weaponPawnOpt = proc->read<uintptr_t>(*weaponBlockOpt + Offsets::EntityList::CHUNK_STRIDE * innerIdx);
                    if (weaponPawnOpt && *weaponPawnOpt) {
                        weaponPawn = *weaponPawnOpt;
                    }
                }
            }
        }

        int shotsFired = 0;
        if (weaponPawn) {
            auto shotsOpt = proc->read<int32_t>(weaponPawn + Offsets::m_iShotsFired::STATIC_PTR);
            if (shotsOpt) shotsFired = *shotsOpt;
        }

        int weaponId = 0;
        if (weaponPawn) {
            auto idOpt = proc->read<int32_t>(weaponPawn + Offsets::m_iItemDefinitionIndex::STATIC_PTR);
            if (idOpt) weaponId = *idOpt;
        }

        const bool isAk = (weaponId == 7 || weaponId == 8);
        const bool isM4 = (weaponId == 16 || weaponId == 17);
        const auto* pattern = (isAk ? &akPattern : (isM4 ? &m4Pattern : nullptr));

        if (pattern && shotsFired > 0 && shotsFired <= 31) {
            auto [patYaw, patPitch] = (*pattern)[shotsFired - 1];
            float targetYaw = -(patYaw - aimPunch.y);
            float targetPitch = -(patPitch - aimPunch.x);

            float rawDx = targetYaw * rcs.strength * rcs.sensitivity;
            float rawDy = targetPitch * rcs.strength * rcs.sensitivity;

            float randomScale = 0.92f + (static_cast<float>(rand() % 19) / 100.0f);
            Vec3 desiredRcs{rawDx * randomScale, rawDy * randomScale, 0.0f};

            Vec3 smoothRcs{
                previousRcsDelta.x + (desiredRcs.x - previousRcsDelta.x) * rcs.smooth,
                previousRcsDelta.y + (desiredRcs.y - previousRcsDelta.y) * rcs.smooth,
                0.0f
            };
            previousRcsDelta = smoothRcs;

            INPUT rcsInput{};
            rcsInput.type = INPUT_MOUSE;
            rcsInput.mi.dwFlags = MOUSEEVENTF_MOVE;
            rcsInput.mi.dx = (int)std::lroundf(smoothRcs.x);
            rcsInput.mi.dy = (int)std::lroundf(smoothRcs.y);
            SendInput(1, &rcsInput, sizeof(INPUT));
        } else {
            previousRcsDelta = {0.0f, 0.0f, 0.0f};
        }
    }

    static uint64_t lastShot = 0;
    uint64_t now = GetTickCount64();

    static bool altHoldActive = false;

    // ALT+LMB mode: issue a single left-down then keep it held until release.
    if (altFire) {
        if (!altHoldActive) {
            INPUT down{0};
            down.type = INPUT_MOUSE;
            down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &down, sizeof(INPUT));
            altHoldActive = true;
        }
        // In this mode, don't send repeated down-up pulses; leave game handling auto-fire.
    } else {
        if (altHoldActive) {
            INPUT up{0};
            up.type = INPUT_MOUSE;
            up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &up, sizeof(INPUT));
            altHoldActive = false;
        }

        bool shouldShoot = false;
        uint64_t usedDelay = normalDelayMs;

        if (leftDown) {
            // Normal hold behavior should be in-game, but send one-shot on initial press to trigger.
            shouldShoot = leftJustPressed;
        }
        if (autoFire && now >= lastShot + normalDelayMs) {
            shouldShoot = true;
            usedDelay = normalDelayMs;
        }

        if (shouldShoot && now >= lastShot + usedDelay) {
            lastShot = now;
            INPUT input[2]{};
            input[0].type = INPUT_MOUSE;
            input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            input[1].type = INPUT_MOUSE;
            input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(2, input, sizeof(INPUT));
        }
    }
}

