#pragma once

#include <array>
#include <string>
#include <vector>
#include <windows.h>
#include "mem.hpp"
#include "offsets.hpp"
#include "gui.hpp"
#include "overlay.hpp"
#include "bomb_found.hpp"

namespace PlayerScanner {

struct Vec3 { float x, y, z; };

bool worldToScreen(const Vec3& world,
                   const std::array<float,16>& m,
                   int width, int height,
                   POINT& out);

bool getCS2WindowRect(RECT& out);

std::string readRemoteString(mem::ProcessMemory* proc,
                             uintptr_t address,
                             size_t maxLen);

struct PlayerRecord {
    int index;
    int team;
    std::string name;
    uint32_t health;

    bool operator==(const PlayerRecord& o) const noexcept;
};

} // namespace PlayerScanner
