#pragma once

#include <cstdint>
#include <string>

#include "mem.hpp"
#include "offsets.hpp"

namespace PlayerScanner {

    std::string readRemoteString(mem::ProcessMemory* proc,
                                 uintptr_t address,
                                 size_t maxLen = 64);

    bool scanPlayers(mem::ProcessMemory* proc);

} // namespace PlayerScanner
