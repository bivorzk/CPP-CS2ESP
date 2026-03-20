#pragma once
#include <cstdint>
#include <vector>

// ============================================================
//  offsets.hpp — Put all your addresses and pointer chains here
//
//  How to use:
//    1. Find the static pointer offset from the module base
//       (e.g. via Cheat Engine pointer scan)
//    2. Fill in the offsets vector with each level of the chain
//    3. Use in main.cpp:
//         uintptr_t base = g_proc->getModuleBase(Offsets::MODULE);
//         uintptr_t addr = g_proc->resolvePointerChain(
//             base + Offsets::Health::STATIC_PTR,
//             Offsets::Health::CHAIN
//         );
//         auto hp = g_proc->read<float>(addr);
// ============================================================

namespace Offsets {

    // The main module to grab the base address from
    inline constexpr const char* MODULE = "game.exe";

    namespace dwEntityList {
        inline constexpr uintptr_t STATIC_PTR = 0x024B7C68;
        inline constexpr std::vector<uintptr_t> CHAIN = { };
    }
    namespace m_hPlayerPawn {
    inline constexpr uintptr_t STATIC_PTR = 0x0000090C;
    inline constexpr std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_iHealth {
        inline constexpr uintptr_t STATIC_PTR = 0x00000354;
        inline constexpr std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_iszPlayerName {
        inline constexpr uintptr_t STATIC_PTR = 0x000006F8;
        inline constexpr std::vector<uintptr_t> CHAIN = { };
    }

    // Add more namespaces below as you find offsets ...

} // namespace Offsets
