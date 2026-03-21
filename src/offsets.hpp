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
    inline constexpr const char* MODULE = "client.dll";

    namespace dwEntityList {
        inline constexpr uintptr_t STATIC_PTR = 38466152;   // entity list pointer
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace dwLocalPlayerController {
        inline constexpr std::ptrdiff_t STATIC_PTR = 0x22F4188; // local player controller offset
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace dwLocalPlayerPawn {
        inline constexpr std::ptrdiff_t STATIC_PTR = 0x2069B50; // local player pawn offset
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace dwViewMatrix {
        inline constexpr uintptr_t STATIC_PTR = 0x230FF20;  // view matrix pointer (example)
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_hPlayerPawn {
    inline constexpr uintptr_t STATIC_PTR = 0x0000090C;
    inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_iHealth {
        inline constexpr uintptr_t STATIC_PTR = 0x00000354; // 852 decimal
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_iTeamNum {
        inline constexpr uintptr_t STATIC_PTR = 0x000003F3; // 1011 decimal
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_bIsLocalPlayerController {
        inline constexpr uintptr_t STATIC_PTR = 0x00000788; // 1928 decimal
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_bPawnIsAlive {
        inline constexpr uintptr_t STATIC_PTR = 0x00000914; // 2324 decimal
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_iPawnHealth {
        inline constexpr uintptr_t STATIC_PTR = 0x00000918; // 2328 decimal
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_iszPlayerName {
        inline constexpr uintptr_t STATIC_PTR = 0x000006F8;
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace EntityList {
        inline constexpr uintptr_t ENTRY_OFFSET = 0x10;
        inline constexpr uintptr_t CHUNK_STRIDE = 0x70;
    }

    namespace m_vecOrigin {
        inline constexpr uintptr_t STATIC_PTR = 0x1588;   // pawn 3D world position (example)
        inline constexpr uintptr_t ABS_ORIGIN = 0x138;    // common abs origin fallback
    }

    namespace m_vecViewOffset {
        inline constexpr uintptr_t STATIC_PTR = 0xD58;   // camera-relative offset (example)
    }

    // Add more namespaces below as you find offsets ...

} // namespace Offsets
