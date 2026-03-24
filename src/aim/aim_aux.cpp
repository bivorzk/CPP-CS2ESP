#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "aim_common.hpp"
#include <cctype>
#include <cmath>
#include <algorithm>

namespace Aim {

bool isCs2ForegroundWindow() {
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
    float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);

    Vec3 ang;
    ang.x = -atan2f(delta.z, hyp) * (180.0f / 3.14159265358979323846f);
    ang.y = atan2f(delta.y, delta.x) * (180.0f / 3.14159265358979323846f);
    ang.z = 0.0f;

    ang.x = clampf01(ang.x, -89.0f, 89.0f);
    ang.y = normalizeYaw(ang.y);

    return ang;
}

Vec3 angleToDirection(const Vec3& ang) {
    float radPitch = ang.x * (3.14159265358979323846f / 180.0f);
    float radYaw = ang.y * (3.14159265358979323846f / 180.0f);
    Vec3 d;
    d.x = cosf(radPitch) * cosf(radYaw);
    d.y = cosf(radPitch) * sinf(radYaw);
    d.z = -sinf(radPitch);
    return d;
}

float dotProduct(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length(const Vec3& v) {
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
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
    if (distance <= 0.001f) return { 180.0f, 0.0f };
    Vec3 toTargetN = normalize(toTarget);
    float dot = std::clamp(dotProduct(forward, toTargetN), -1.0f, 1.0f);
    float fovDeg = acosf(dot) * (180.0f / 3.14159265358979323846f);
    return { fovDeg, distance };
}

float distance3d(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

bool isPawnAlive(mem::ProcessMemory* proc, uintptr_t pawn) {
    if (!proc || !pawn) return false;
    auto aliveOpt = proc->read<uint8_t>(pawn + Offsets::m_bPawnIsAlive::STATIC_PTR);
    if (aliveOpt) {
        return (*aliveOpt != 0);
    }
    auto healthOpt = proc->read<int32_t>(pawn + Offsets::m_iPawnHealth::STATIC_PTR);
    return (healthOpt && *healthOpt > 0);
}

std::vector<int> boneCandidatesFor(int aimPart) {
    switch (aimPart) {
        case 0: return { 6, 5 };
        case 1: return { 4, 2, 0, 5 };
        case 2: return { 8, 9, 11, 13, 14, 16 };
        case 3: return { 22, 23, 24, 25, 26, 27 };
        default: return { 6, 5, 4, 0 };
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
    bool localIdValid = (localPlayerId >= 0 && localPlayerId < 32);
    if (hasMask && localIdValid) spottedByLocal = (((*maskOpt) >> localPlayerId) & 1u) != 0;

    bool visible = false;
    if (strictMaskOnly) {
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

bool worldToScreen(const Vec3& world,
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

bool getCS2WindowRect(RECT& out) {
    HWND hCS2 = FindWindowA("Valve001", nullptr);
    if (!hCS2) hCS2 = FindWindowA(nullptr, "Counter-Strike 2");
    if (!hCS2) return false;

    if (!GetWindowRect(hCS2, &out)) return false;
    return true;
}

int getLocalPlayerId(mem::ProcessMemory* proc, uintptr_t entityList, uintptr_t localPawn) {
    if (!proc || !entityList || !localPawn) return -1;
    for (int i = 0; i < 64; ++i) {
        auto entryOpt = proc->read<uintptr_t>(entityList + 0x8 * i + Offsets::EntityList::ENTRY_OFFSET);
        if (!entryOpt || *entryOpt == 0) continue;
        uintptr_t entry = *entryOpt;
        auto pawnOpt = proc->read<uintptr_t>(entry + Offsets::EntityList::CHUNK_STRIDE * i);
        if (!pawnOpt || *pawnOpt == 0) continue;
        if (*pawnOpt == localPawn) return i;
    }
    return -1;
}

bool getRawBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, int boneId, Vec3& out) {
    (void)proc;
    (void)pawn;
    (void)boneId;
    (void)out;
    return false;
}

bool getBoneWorldPosition(mem::ProcessMemory* proc, uintptr_t pawn, const Vec3& origin, int aimPart, Vec3& out) {
    auto candidates = boneCandidatesFor(aimPart);
    for (int bone : candidates) {
        if (getRawBoneWorldPosition(proc, pawn, bone, out)) {
            return true;
        }
    }
    return false;
}

Vec3 selectAimPoint(const Vec3& origin, const Vec3& viewOffset, int aimPart) {
    Vec3 point = origin;
    point.x += viewOffset.x;
    point.y += viewOffset.y;
    point.z += viewOffset.z;
    return point;
}

float calcAimbotScore(float fov, float dist, int health) {
    // lower score is better
    return fov * 0.7f + dist * 0.2f - std::clamp<float>(health, 0, 100) * 0.05f;
}

float angdiff(float a, float b) {
    float diff = a - b;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return diff;
}

Vec3 smoothAim(const Vec3& current, const Vec3& desired, float fraction) {
    return {
        current.x + (desired.x - current.x) * fraction,
        current.y + (desired.y - current.y) * fraction,
        0.0f
    };
}

void moveMouseForAngleDelta(const Vec3& delta, float sensitivity) {
    const float M_YAW = 0.022f;
    float sens = sensitivity > 0.01f ? sensitivity : 1.0f;
    float pixels_per_deg = 1.0f / (sens * M_YAW);

    int dx = (int)roundf(-delta.y * pixels_per_deg);
    int dy = (int)roundf(delta.x * pixels_per_deg);

    if (dx == 0 && dy == 0) return;

    if (dx == 0 && fabsf(delta.y) > 0.005f) dx = (delta.y > 0) ? -1 : 1;
    if (dy == 0 && fabsf(delta.x) > 0.005f) dy = (delta.x > 0) ? 1 : -1;

    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    SendInput(1, &input, sizeof(INPUT));
}

} // namespace Aim
