#include "player_scanner_helpers.hpp"
#include <algorithm>
#include <cmath>

namespace PlayerScanner {

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
    GetWindowRect(hCS2, &out);
    return true;
}

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

bool PlayerRecord::operator==(const PlayerRecord& o) const noexcept {
    return index == o.index && team == o.team && health == o.health && name == o.name;
}

} // namespace PlayerScanner
