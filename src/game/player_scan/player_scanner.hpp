#pragma once

#include <cstdint>
#include <string>

#include "mem.hpp"
#include "offsets.hpp"
#include "bomb_found.hpp"

namespace PlayerScanner {

    std::string readRemoteString(mem::ProcessMemory* proc,
                                 uintptr_t address,
                                 size_t maxLen = 64);

    bool scanPlayers(mem::ProcessMemory* proc, const Bomb::Info* bombInfo = nullptr);

} // namespace PlayerScanner
