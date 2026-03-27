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
        inline constexpr uintptr_t STATIC_PTR = 0x24B0258;   // entity list pointer
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace dwLocalPlayerController {
        inline constexpr std::ptrdiff_t STATIC_PTR = 0x22F5028; // local player controller offset
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace dwLocalPlayerPawn {
        inline constexpr std::ptrdiff_t STATIC_PTR = 0x206A9E0; // local player pawn offset
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace dwViewMatrix {
        inline constexpr uintptr_t STATIC_PTR = 0x2310F10;  // view matrix pointer (example)
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

    namespace m_vecAbsVelocity {
        inline constexpr uintptr_t STATIC_PTR = 0x3E0;  // pawn absolute velocity (example/needs validation)
    }



    namespace m_vecViewOffset {
        inline constexpr uintptr_t STATIC_PTR = 0xD58;   // camera-relative offset (example)
    }

    namespace m_angEyeAngles {
        inline constexpr uintptr_t STATIC_PTR = 0x3DD0; // C_CSPlayerPawn m_angEyeAngles (dec 15824)
    }

    namespace m_bSpotted {
        inline constexpr uintptr_t STATIC_PTR = 0x3E5; // best-effort CS2 spotted flag; may require tuning
    }

    namespace m_bDormant {
        inline constexpr uintptr_t STATIC_PTR = 0x00A0; // typical C_BaseEntity m_bDormant (verify for CS2)
    }

    namespace m_bSpottedByMask {
        inline constexpr uintptr_t STATIC_PTR = 0xC; // best-effort CS2 spotted mask; may require tuning
    }

    namespace m_flSimulationTime {
        inline constexpr uintptr_t STATIC_PTR = 0x3C0;   // fallback: example (from local player class maybe C_BaseEntity?)
    }

    namespace m_aimPunchAngle {
        inline constexpr uintptr_t STATIC_PTR = 0x1490; // C_CSPlayerPawn m_aimPunchAngle (approx + active weapon recoil)
    }

    namespace m_pWeaponServices {
        inline constexpr uintptr_t STATIC_PTR = 0x11C0; // local pawn weapon services container
    }

    namespace m_hActiveWeapon {
        inline constexpr uintptr_t STATIC_PTR = 0x1388; // C_CSPlayerPawn->m_hActiveWeapon handle
    }

    namespace m_iShotsFired {
        inline constexpr uintptr_t STATIC_PTR = 0x147C; // C_CSWeaponBase shots fired in current spray
    }

    namespace m_iItemDefinitionIndex {
        inline constexpr uintptr_t STATIC_PTR = 0x1480; // C_CSWeaponBase item definition ID
    }

    namespace C_BaseEntity {
        inline constexpr uintptr_t m_pGameSceneNode = 0x338; // base pointer to CGameSceneNode (from client_dll.json, was 0x334)
    }

    namespace CGameSceneNode {
        inline constexpr uintptr_t m_vecAbsOrigin = 0xD0;  // absolute world origin
        inline constexpr uintptr_t m_vecOrigin    = 0x88;  // net quantized origin, may require conversion
    }

    namespace C_PlantedC4 {
        inline constexpr uintptr_t m_bBombTicking = 0x1170;
        inline constexpr uintptr_t m_nBombSite = 0x1174;
        inline constexpr uintptr_t m_bC4Activated = 0x11B8; // updated per dump
        inline constexpr uintptr_t m_bBeingDefused = 0x11AC;
        inline constexpr uintptr_t m_bBombDefused = 0x11C4;
        inline constexpr uintptr_t m_flC4Blow = 0x11A0;
        inline constexpr uintptr_t m_hBombDefuser = 0x11D0; // m_hBombDefuser from client json 4552
        inline constexpr uintptr_t m_vecC4ExplodeSpectatePos = 0x5824; // world position for bomb overlay (from client_dll.json)
    }

    namespace dwPlantedC4 {
        inline constexpr uintptr_t STATIC_PTR = 0x2318A60;  // planted C4 list pointer
        inline const std::vector<uintptr_t> CHAIN = { };
    }
    namespace m_bBombPlanted {
        inline constexpr uintptr_t STATIC_PTR = 0x00001170; // C_PlantedC4 m_bBombTicking (updated per dump)
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_pGameSceneNode {
        inline constexpr uintptr_t STATIC_PTR = 0x338; 
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace m_modelState {
        inline constexpr uintptr_t STATIC_PTR = 0x160; 
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    namespace boneArrayOffset {
        inline constexpr uintptr_t STATIC_PTR = 0x80;
        inline const std::vector<uintptr_t> CHAIN = { };
    }
    namespace boneStride {
        inline constexpr uintptr_t STATIC_PTR = 0x80;
        inline const std::vector<uintptr_t> CHAIN = { };
    }
    namespace boneCount {
        inline constexpr uintptr_t STATIC_PTR = 28;
        inline const std::vector<uintptr_t> CHAIN = { };
    }

    // m_pGameSceneNode = 0x338
    // m_modelState = 0x160

    // Add more namespaces below as you find offsets ...

} // namespace Offsets
