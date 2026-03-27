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

static Vec3 angleToDirection(const Vec3& ang) {
    float radPitch = ang.x * (3.14159265358979323846f / 180.0f);
    float radYaw = ang.y * (3.14159265358979323846f / 180.0f);
    Vec3 d;
    d.x = cosf(radPitch) * cosf(radYaw);
    d.y = cosf(radPitch) * sinf(radYaw);
    d.z = -sinf(radPitch);
    return d;
}

static float dotProduct(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float length(Vec3 v) {
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

static Vec3 normalize(Vec3 v) {
    float len = length(v);
    if (len <= 1e-6f) return {0.0f,0.0f,0.0f};
    return {v.x/len, v.y/len, v.z/len};
}

struct FovResult { float fovDeg; float distance; };

static FovResult GetTargetFOVAndDistance(const Vec3 &eyePos, const Vec3 &viewDir, const Vec3 &targetPos) {
    Vec3 forward = normalize(viewDir);
    Vec3 toTarget{targetPos.x - eyePos.x, targetPos.y - eyePos.y, targetPos.z - eyePos.z};
    float distance = length(toTarget);
    if (distance <= 0.001f) return { 180.0f, 0.0f };
    Vec3 toTargetN = normalize(toTarget);
    float dot = std::clamp(dotProduct(forward, toTargetN), -1.0f, 1.0f);
    float fovDeg = acosf(dot) * (180.0f / 3.14159265358979323846f);
    return { fovDeg, distance };
}

static float distance3d(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

// Stateful velocity approximation for local and target movement
static Vec3 g_prevLocalPos = {0.0f, 0.0f, 0.0f};
static uint64_t g_prevLocalTimeMs = 0;
static uintptr_t g_prevTargetPawn = 0;
static Vec3 g_prevTargetPos = {0.0f, 0.0f, 0.0f};

static bool isPawnAlive(mem::ProcessMemory* proc, uintptr_t pawn) {
    if (!proc || !pawn) return false;
    auto aliveOpt = proc->read<uint8_t>(pawn + Offsets::m_bPawnIsAlive::STATIC_PTR);
    if (aliveOpt) {
        return (*aliveOpt != 0);
    }
    auto healthOpt = proc->read<int32_t>(pawn + Offsets::m_iPawnHealth::STATIC_PTR);
    return (healthOpt && *healthOpt > 0);
}

static constexpr float TRIGGER_FOV = 1.5f;
static constexpr float BONE_Z_SANITY = 150.0f;
static constexpr float SWITCH_HYSTERESIS = 0.85f;

static std::vector<int> boneCandidatesFor(int aimPart) {
    // BoneID: Head=6, Neck=5, Spine=4, Spine1=2, Pelvis=0,
    //   LeftShoulder=8, LeftArm=9, LeftHand=11,
    //   RightShoulder=13, RightArm=14, RightHand=16,
    //   LeftHip=22, LeftKnee=23, LeftFoot=24,
    //   RightHip=25, RightKnee=26, RightFoot=27
    switch (aimPart) {
        case 0: // head
            return { 6, 5 };
        case 1: // body
            return { 4, 2, 0, 5 };
        case 2: // arms
            return { 8, 9, 11, 13, 14, 16 };
        case 3: // legs
            return { 22, 23, 24, 25, 26, 27 };
        default:
            return { 6, 5, 4, 0 };
    }
}

static bool isPawnVisible(mem::ProcessMemory* proc, uintptr_t pawn, int localPlayerId, bool strictMaskOnly = false) {
    // debug override: allow visibility checks to be bypassed when target selection fails unexpectedly.
    static bool g_forceVisible = true;
    if (g_forceVisible) {
        return true;
    }

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
        } else {
            // No visibility info available, assume valid when strict visibility is off.
            visible = true;
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

static float angdiff(float a, float b); // forward declaration for smoothAim

static float calcAimbotScore(float fov, float dist, uint32_t health) {
    // Prioritize closest enemies in FOV, with small health weighting to finish low-HP targets.
    const float fovWeight = 1.0f;
    const float distWeight = 0.025f;
    const float healthWeight = 0.015f;
    return fov * fovWeight + dist * distWeight + static_cast<float>(health) * healthWeight;
}

static Vec3 smoothAim(const Vec3& current, const Vec3& target, float factor) {
    float deltaYaw = angdiff(target.y, current.y);
    float deltaPitch = angdiff(target.x, current.x);
    return { deltaPitch * factor, deltaYaw * factor, 0.0f };
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

static Vec3 selectAimPoint(const Vec3& origin, const Vec3& viewOffset, int aimPart) {
    switch (aimPart) {
        case 1: // body
            return { origin.x, origin.y, origin.z + viewOffset.z * 0.55f };
        case 2: // arms
            return { origin.x, origin.y, origin.z + viewOffset.z * 0.45f };
        case 3: // legs
            return { origin.x, origin.y, origin.z + viewOffset.z * 0.15f };
        case 0: // head
        default:
            return { origin.x + viewOffset.x, origin.y + viewOffset.y, origin.z + viewOffset.z };
    }
}

static bool getRawBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, int boneIndex, Vec3& out) {
    if (!proc || !pawn || boneIndex < 0) return false;

    // Step 1: pawn -> m_pGameSceneNode (pointer)
    auto gameSceneNodeOpt = proc->read<uintptr_t>(pawn + Offsets::m_pGameSceneNode::STATIC_PTR);
    if (!gameSceneNodeOpt || *gameSceneNodeOpt == 0) {
        Gui::log("[DBG] bone: gameSceneNode read failed for pawn 0x%llX", (unsigned long long)pawn);
        return false;
    }
    uintptr_t gameSceneNode = *gameSceneNodeOpt;

    // Step 2: gameSceneNode + m_modelState + boneArrayOffset -> bone array pointer
    uintptr_t boneArrayAddr = gameSceneNode + Offsets::m_modelState::STATIC_PTR + Offsets::boneArrayOffset::STATIC_PTR;
    auto boneArrayPtrOpt = proc->read<uintptr_t>(boneArrayAddr);
    if (!boneArrayPtrOpt || *boneArrayPtrOpt == 0) {
        Gui::log("[DBG] bone: boneArray read failed at 0x%llX (GSN=0x%llX)", (unsigned long long)boneArrayAddr, (unsigned long long)gameSceneNode);
        return false;
    }
    uintptr_t boneArray = *boneArrayPtrOpt;

    // Step 3: boneArray + boneIndex * stride -> Vec3 position
    // Standard CS2 bone stride is 32 bytes (0x20): 3 floats pos + 1 float scale + 4 floats quat
    constexpr uintptr_t BONE_STRIDE = 0x20;
    uintptr_t boneEntry = boneArray + static_cast<uintptr_t>(boneIndex) * BONE_STRIDE;

    auto bonePosOpt = proc->read<Vec3>(boneEntry);
    if (!bonePosOpt) {
        Gui::log("[DBG] bone: pos read failed at 0x%llX (bone=%d)", (unsigned long long)boneEntry, boneIndex);
        return false;
    }

    out = *bonePosOpt;
    return true;
}

static bool getBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, const Vec3& origin, int aimPart, Vec3& out) {
    auto candidates = boneCandidatesFor(aimPart);

    for (int boneIndex : candidates) {
        Vec3 candidatePos;
        if (!getRawBoneWorldPosition(proc, pawn, boneIndex, candidatePos)) continue;

        float boneDist = distance3d(origin, candidatePos);
        if (boneDist > BONE_Z_SANITY) {
            Gui::log("[DBG] pawn=0x%llX candidate bone %d distance %.1f > sanity %.1f", (unsigned long long)pawn, boneIndex, boneDist, BONE_Z_SANITY);
            continue;
        }

        out = candidatePos;
        Gui::log("[DBG] pawn=0x%llX selected bone=%d distance=%.1f", (unsigned long long)pawn, boneIndex, boneDist);
        return true;
    }

    // fallback options
    if (getRawBoneWorldPosition(proc, pawn, 6, out)) {
        float boneDist = distance3d(origin, out);
        if (boneDist <= BONE_Z_SANITY) {
            Gui::log("[DBG] pawn=0x%llX fallback head bone selected distance=%.1f", (unsigned long long)pawn, boneDist);
            return true;
        }
        Gui::log("[DBG] pawn=0x%llX fallback head bone outside sanity %.1f", (unsigned long long)pawn, boneDist);
    }

    return false;
}

static void moveMouseForAngleDelta(const Vec3& delta, float sensitivity) {
    // compatibility with CS2 mouse scaling, approximates rust M_YAW behavior.
    const float M_YAW = 0.022f;
    float sens = sensitivity > 0.01f ? sensitivity : 1.0f;
    float pixels_per_deg = 1.0f / (sens * M_YAW);

    int dx = (int)roundf(-delta.y * pixels_per_deg); // right negative in Source
    int dy = (int)roundf(delta.x * pixels_per_deg);

    if (dx == 0 && dy == 0) return;

    // Ensure minimum 1px movement when there's a real delta
    if (dx == 0 && fabsf(delta.y) > 0.005f) dx = (delta.y > 0) ? -1 : 1;
    if (dy == 0 && fabsf(delta.x) > 0.005f) dy = (delta.x > 0) ? 1 : -1;

    // Add small timing jitter (1-2 ms) to avoid perfectly deterministic motion patterns.
    int jitterMs = (rand() % 2) + 1;
    Sleep(jitterMs);

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    SendInput(1, &input, sizeof(INPUT));
}

void Aim::update(mem::ProcessMemory* proc) {
    if (!proc) return;

    bool cs2Foreground = isCs2ForegroundWindow();
    bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool autoAim = Gui::getVisuals().autoAim;
    bool altAutoFire = Gui::getVisuals().altAutoFire;
    bool autoFire = autoAim || (altDown && altAutoFire); // auto-fire: autoAim always, ALT only if altAutoFire enabled
    bool aimHold = true; // if true, ALT or auto-aim required to aim mode
    bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool altHold = altDown;
    bool keyHeld = altHold;
    // left click alone should NOT activate aimbot (only normal manual fire)
    bool shouldAct = (aimHold ? keyHeld : true) && (autoAim || altHold);

    if (!cs2Foreground || !shouldAct) return;

    bool isFiring = false; // placeholder; can implement weapon-shot query after entityList is known.
    bool fireOnly = false; // in Rust this is AimMode::FireOnly
    if (fireOnly && !isFiring) return;

    Gui::log("[DBG] Aim::update autoAim=%d altDown=%d keyHeld=%d shouldAct=%d isFiring=%d", autoAim, altDown, keyHeld, shouldAct, isFiring);

    // On pass, final aim & trigger behavior continues.

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
    if (!base) { Gui::log("[DBG] aim: client.dll base=0"); return; }

    auto localPawnOpt = proc->read<uintptr_t>(base + Offsets::dwLocalPlayerPawn::STATIC_PTR);
    if (!localPawnOpt || *localPawnOpt == 0) { Gui::log("[DBG] aim: localPawn=0"); return; }
    uintptr_t localPawn = *localPawnOpt;

    auto localTeamOpt = proc->read<int32_t>(localPawn + Offsets::m_iTeamNum::STATIC_PTR);
    auto localOriginOpt = proc->read<Vec3>(localPawn + Offsets::m_vecOrigin::STATIC_PTR);
    auto localViewOffsetOpt = proc->read<Vec3>(localPawn + Offsets::m_vecViewOffset::STATIC_PTR);
    if (!localTeamOpt || !localOriginOpt || !localViewOffsetOpt) {
        Gui::log("[DBG] aim: local team/origin/viewOff failed (team=%d origin=%d viewOff=%d)",
                 localTeamOpt.has_value(), localOriginOpt.has_value(), localViewOffsetOpt.has_value());
        return;
    }

    int localTeam = *localTeamOpt;
    Vec3 eyeOrigin = *localOriginOpt;
    eyeOrigin.x += localViewOffsetOpt->x;
    eyeOrigin.y += localViewOffsetOpt->y;
    eyeOrigin.z += localViewOffsetOpt->z;

    Gui::log("[DBG] aim: localTeam=%d eye=(%.1f,%.1f,%.1f)", localTeam, eyeOrigin.x, eyeOrigin.y, eyeOrigin.z);

    auto entityListOpt = proc->read<uintptr_t>(base + Offsets::dwEntityList::STATIC_PTR);
    if (!entityListOpt || *entityListOpt == 0) { Gui::log("[DBG] aim: entityList=0"); return; }
    uintptr_t entityList = *entityListOpt;

    auto firstEntityOpt = proc->read<uintptr_t>(entityList + Offsets::EntityList::ENTRY_OFFSET);
    if (!firstEntityOpt || *firstEntityOpt == 0) { Gui::log("[DBG] aim: firstEntity=0"); return; }
    uintptr_t firstEntity = *firstEntityOpt;

    int localPlayerId = getLocalPlayerId(proc, entityList, localPawn);
    if (localPlayerId < 0) localPlayerId = -1;

    RECT gameRect;
    bool haveMatrix = false;
    std::array<float,16> viewMatrix{};
    int gameW = 0, gameH = 0;

    if (getCS2WindowRect(gameRect)) {
        gameW = gameRect.right - gameRect.left;
        gameH = gameRect.bottom - gameRect.top;
    }

    auto vmOpt = proc->read<std::array<float,16>>(base + Offsets::dwViewMatrix::STATIC_PTR);
    if (vmOpt) {
        viewMatrix = *vmOpt;
        haveMatrix = (gameW > 0 && gameH > 0);
    }

    const auto& visCfg = Gui::getVisuals();
    const float maxRange = 1700.0f;
    const float maxFov = 180.0f; // debug: wide open for testing
    const float strictMaxRange = 1500.0f;
    const float strictMaxYaw = 70.0f;
    const float strictMaxPitch = 70.0f;
    const float aimSmoothFactor = Gui::getVisuals().autoAim ? 2.0f : 3.0f; // fast enough to track

    Vec3 bestTargetHead {0.0f, 0.0f, 0.0f};
    Vec3 bestTargetVelocity{0.0f, 0.0f, 0.0f};
    float bestScore = 1e9f;
    bool found = false;
    uintptr_t chosenPawn = 0;
    Vec3 targetVelocity{0.0f, 0.0f, 0.0f};

    uint64_t nowMs = GetTickCount64();
    float dt = 0.016f;
    if (g_prevLocalTimeMs != 0) {
        dt = (nowMs - g_prevLocalTimeMs) * 0.001f;
        if (dt < 0.001f) dt = 0.001f;
    }

    Vec3 localVelocity{0.0f, 0.0f, 0.0f};
    if (g_prevLocalTimeMs != 0) {
        localVelocity.x = (eyeOrigin.x - g_prevLocalPos.x) / dt;
        localVelocity.y = (eyeOrigin.y - g_prevLocalPos.y) / dt;
        localVelocity.z = (eyeOrigin.z - g_prevLocalPos.z) / dt;
    }
    g_prevLocalPos = eyeOrigin;
    g_prevLocalTimeMs = nowMs;

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
        if (!isPawnAlive(proc, pawn)) {
            Gui::log("[DBG] pawn=0x%llX skipping dead pawn", (unsigned long long)pawn);
            return false;
        }

        if (!visible) return false;

        Vec3 target;
        if (!getBoneWorldPosition(proc, pawn, *originOpt, visCfg.aimPart, target)) {
            target = selectAimPoint(*originOpt, *viewOffsetOpt, visCfg.aimPart);
        }

        float dist = distance3d(eyeOrigin, target);
        return dist <= maxRange;
    };

    auto curAngOpt = proc->read<Vec3>(localPawn + Offsets::m_angEyeAngles::STATIC_PTR);
    if (!curAngOpt) { Gui::log("[DBG] aim: curAng read failed"); return; }
    Vec3 curAng = *curAngOpt;
    Gui::log("[DBG] aim: curAng=(%.2f,%.2f) scanning entities...", curAng.x, curAng.y);

    // Candidate target stickiness & delayed release (optional): set to 0 for immediate reaction.
    static uintptr_t candidatePawn = 0;
    static int candidateFrames = 0;
    static int lostFrames = 0;
    int framesRequired = std::max(0, Gui::getVisuals().visCooldownFrames);

    if (g_targetPawn && isPawnVisible(proc, g_targetPawn, localPlayerId, visCfg.strictVisibility)) {
        Vec3 targetPoint;
        auto originOpt = proc->read<Vec3>(g_targetPawn + Offsets::m_vecOrigin::STATIC_PTR);
        auto viewOffsetOpt = proc->read<Vec3>(g_targetPawn + Offsets::m_vecViewOffset::STATIC_PTR);
        if (originOpt && viewOffsetOpt) {
            if (!getBoneWorldPosition(proc, g_targetPawn, *originOpt, visCfg.aimPart, targetPoint)) {
                targetPoint = selectAimPoint(*originOpt, *viewOffsetOpt, visCfg.aimPart);
            }
        }

        if (targetPoint.x != 0.0f || targetPoint.y != 0.0f || targetPoint.z != 0.0f) {
            if (!found) {
                bestTargetHead = targetPoint;
                found = true;
                chosenPawn = g_targetPawn;
            }
        }
    }

    int aimEntityCount = 0;
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
            localPlayerId = static_cast<int>(pawnIdx);
            continue;
        }

        aimEntityCount++;
        auto teamOpt = proc->read<int32_t>(pawn + Offsets::m_iTeamNum::STATIC_PTR);
        auto hpOpt = proc->read<int32_t>(pawn + Offsets::m_iHealth::STATIC_PTR);
        int readTeam = teamOpt ? *teamOpt : -1;
        int readHp = hpOpt ? *hpOpt : -1;

        Gui::log("[DBG] aim[%d] pawn=0x%llX team=%d(local=%d) hp=%d", i, (unsigned long long)pawn, readTeam, localTeam, readHp);

        if (!teamOpt || *teamOpt == localTeam) { Gui::log("[DBG]  -> skip: same team"); continue; }
        if (!hpOpt || *hpOpt <= 0) { Gui::log("[DBG]  -> skip: dead hp=%d", readHp); continue; }

        auto originOpt = proc->read<Vec3>(pawn + Offsets::m_vecOrigin::STATIC_PTR);
        auto viewOffsetOptEnemy = proc->read<Vec3>(pawn + Offsets::m_vecViewOffset::STATIC_PTR);
        if (!originOpt || !viewOffsetOptEnemy) { Gui::log("[DBG]  -> skip: no origin/viewOff"); continue; }

        Gui::log("[DBG]  -> origin=(%.1f,%.1f,%.1f)", originOpt->x, originOpt->y, originOpt->z);

        Vec3 targetPoint;
        if (!getBoneWorldPosition(proc, pawn, *originOpt, visCfg.aimPart, targetPoint)) {
            targetPoint = selectAimPoint(*originOpt, *viewOffsetOptEnemy, visCfg.aimPart);
            Gui::log("[DBG]  -> bone failed, using fallback");
        } else {
            Gui::log("[DBG]  -> bone OK target=(%.1f,%.1f,%.1f)", targetPoint.x, targetPoint.y, targetPoint.z);
        }

        FovResult fovInfo = GetTargetFOVAndDistance(eyeOrigin, angleToDirection(curAng), targetPoint);
        if (fovInfo.distance <= 0.0f || fovInfo.distance > maxRange) continue;

        // dynamic FOV scaling by distance (reference from PureLiquid CS2)
        float dynamicFov = maxFov / (fovInfo.distance / 100.0f + 1.0f);
        if (fovInfo.fovDeg > dynamicFov) {
            Gui::log("[DBG] candidate skip pawn=0x%llX fov=%.2f dynamicFov=%.2f dist=%.2f", (unsigned long long)pawn, fovInfo.fovDeg, dynamicFov, fovInfo.distance);
            continue;
        }

        int health = hpOpt ? *hpOpt : 100;
        float score = calcAimbotScore(fovInfo.fovDeg, fovInfo.distance, health);
        Vec3 candidateVelocity{0.0f, 0.0f, 0.0f};
        auto velOpt = proc->read<Vec3>(pawn + Offsets::m_vecAbsVelocity::STATIC_PTR);
        if (velOpt) candidateVelocity = *velOpt;

        if (!found || score < bestScore) {
            found = true;
            bestScore = score;
            bestTargetHead = targetPoint;
            bestTargetVelocity = candidateVelocity;
            chosenPawn = pawn;
        }
    }

    Gui::log("[DBG] aim loop done: %d entities checked, found=%d", aimEntityCount, (int)found);

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

    // Estimate target movement velocity (based on previous frame where entity was same target pawn)
    if (g_targetPawn == g_prevTargetPawn && g_targetPawn != 0) {
        targetVelocity.x = (bestTargetHead.x - g_prevTargetPos.x) / dt;
        targetVelocity.y = (bestTargetHead.y - g_prevTargetPos.y) / dt;
        targetVelocity.z = (bestTargetHead.z - g_prevTargetPos.z) / dt;
    } else {
        targetVelocity = {0.0f, 0.0f, 0.0f};
    }
    g_prevTargetPawn = g_targetPawn;
    g_prevTargetPos = bestTargetHead;

    if (!g_targetPawn) {
        return;
    }

    if (g_targetPawn != chosenPawn && !found) {
        // Re-validate the sticky target is still alive before aiming
        auto stickyHpOpt = proc->read<int32_t>(g_targetPawn + Offsets::m_iHealth::STATIC_PTR);
        if (!stickyHpOpt || *stickyHpOpt <= 0) {
            Gui::log("[DBG] sticky target 0x%llX is dead (hp=%d), dropping", (unsigned long long)g_targetPawn, stickyHpOpt ? *stickyHpOpt : -1);
            g_targetPawn = 0;
            g_pendingPawn = 0;
            g_pendingFrames = 0;
            return;
        }
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

    // Predict target movement and compensate for own movement.
    float travelTime = distance3d(eyeOrigin, bestTargetHead) / 12000.0f;
    if (travelTime > 0.2f) travelTime = 0.2f;

    Vec3 leadTarget{
        bestTargetHead.x + bestTargetVelocity.x * travelTime - localVelocity.x * travelTime,
        bestTargetHead.y + bestTargetVelocity.y * travelTime - localVelocity.y * travelTime,
        bestTargetHead.z + bestTargetVelocity.z * travelTime - localVelocity.z * travelTime
    };

    Vec3 desiredAng = calcAngle(eyeOrigin, leadTarget);
    Vec3 deltaStep = smoothAim(curAng, desiredAng, 1.0f / aimSmoothFactor);

    Gui::log("[DBG] aiming pawn=0x%llX fov=%.2f dist=%.1f curAng=(%.2f,%.2f) desiredAng=(%.2f,%.2f) delta=(%.2f,%.2f)",
             (unsigned long long)g_targetPawn,
             GetTargetFOVAndDistance(eyeOrigin, angleToDirection(curAng), bestTargetHead).fovDeg,
             distance3d(eyeOrigin, bestTargetHead),
             curAng.x, curAng.y,
             desiredAng.x, desiredAng.y,
             deltaStep.x, deltaStep.y);

    if (fabsf(deltaStep.x) < 0.01f && fabsf(deltaStep.y) < 0.01f) {
        Gui::log("[DBG] delta too small, skipping mouse input");
    } else {
        // Do not write view angles directly in CS2; use mouse motion to make change apply naturally.
        moveMouseForAngleDelta(deltaStep, 1.0f);
    }

    // Keep head points for overlay if needed, but no extra input required.
    (void)base;
    (void)curAng;


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

    // Guard: don't shoot at dead enemies
    if (g_targetPawn) {
        auto tgtHpOpt = proc->read<int32_t>(g_targetPawn + Offsets::m_iHealth::STATIC_PTR);
        if (!tgtHpOpt || *tgtHpOpt <= 0) {
            Gui::log("[DBG] target dead before shoot, dropping 0x%llX", (unsigned long long)g_targetPawn);
            g_targetPawn = 0;
            g_pendingPawn = 0;
            g_pendingFrames = 0;
            return;
        }
    }

    static uint64_t lastShot = 0;
    uint64_t now = GetTickCount64();

    // Aim-fire mode active for auto-aim or ALT hold
    bool shouldShoot = false;
    uint64_t usedDelay = normalDelayMs;

    if (autoFire) {
        // shoot at cadence when aim mode is active
        if (now >= lastShot + normalDelayMs) {
            shouldShoot = true;
            usedDelay = normalDelayMs;
        }
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

