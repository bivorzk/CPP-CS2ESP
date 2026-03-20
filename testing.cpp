#include <windows.h>
#include <TlHelp32.h>     // for module snapshot
#include <iostream>
#include <string>
#include <iomanip>        // for std::setw, std::left
#include <cstdint>        // ← ADD THIS: uint32_t, uintptr_t, int32_t, etc.

// ───────────────────────────────────────────────
//          IMPORTANT: UPDATE THESE OFFSETS!
//          Get fresh values from a2x/cs2-dumper or similar
//          (as of March 2026 — they change often!)
// ───────────────────────────────────────────────


/*
    int dwEntityList = 38466152;
    int m_hPlayerPawn = 0x90C;
    int m_iHealth = 0x354;
    int m_iszPlayerName = 0x6F8;
*/


constexpr uintptr_t dwEntityList    = 38466152;   // ← REPLACE! (example old ~38M; check latest dump)
constexpr uintptr_t m_hPlayerPawn   = 0x90C;       // CHandle<C_CSPlayerPawn>
constexpr uintptr_t m_iHealth       = 0x354;       // int32
constexpr uintptr_t m_iszPlayerName = 0x6F8;       // const char* or std::string-ish

constexpr uintptr_t ENTITY_ENTRY_OFFSET = 0x10;    // usually start of list entries
constexpr uintptr_t CHUNK_STRIDE        = 0x70;    // common controller/pawn stride in 2026

HANDLE hProcess   = nullptr;
uintptr_t clientBase = 0;

// Helper: Read memory safely
template<typename T>
bool ReadMemory(uintptr_t address, T& out, size_t size = sizeof(T)) {
    if (address == 0) return false;
    return ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), &out, size, nullptr);
}

// Read null-terminated string (safe, with max length)
std::string ReadString(uintptr_t address, const size_t maxLen = 64) {
    if (address == 0) return "";
    char buffer[128] = {0};
    size_t bytesToRead = std::min(maxLen, sizeof(buffer) - 1);
    if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), buffer, bytesToRead, nullptr)) {
        return "";
    }
    buffer[bytesToRead] = '\0'; // ensure termination
    return std::string(buffer);
}

// Get module base address (narrow string version)
uintptr_t GetModuleBase(DWORD processId, const std::string& moduleName) {
    uintptr_t baseAddress = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cout << "CreateToolhelp32Snapshot failed: " << GetLastError() << "\n";
        return 0;
    }

    MODULEENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    if (Module32First(snapshot, &entry)) {
        do {
            if (_stricmp(moduleName.c_str(), entry.szModule) == 0) {
                baseAddress = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return baseAddress;
}

int main() {
    // ──────────────────────────────
    //   Find CS2 window & process
    // ──────────────────────────────
    HWND hwnd = FindWindowA(nullptr, "Counter-Strike 2");  // more reliable title
    if (!hwnd) {
        std::cout << "CS2 window not found! Make sure the game is running.\n";
        return 1;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        std::cout << "Failed to get process ID.\n";
        return 1;
    }

    hProcess = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) {
        std::cout << "Failed to open process (error: " << GetLastError() << "). Run as administrator?\n";
        return 1;
    }

    // ──────────────────────────────
    //   Get client.dll base address
    // ──────────────────────────────
    clientBase = GetModuleBase(pid, "client.dll");
    if (clientBase == 0) {
        std::cout << "Failed to find client.dll in process.\n";
        CloseHandle(hProcess);
        return 1;
    }

    std::cout << "Attached to CS2 (PID: " << pid << ")\n";
    std::cout << "client.dll base: 0x" << std::hex << clientBase << "\n\n";

    // ──────────────────────────────
    //   Read entity list pointer
    // ──────────────────────────────
    uintptr_t entityList = 0;
    if (!ReadMemory(clientBase + dwEntityList, entityList) || entityList == 0) {
        std::cout << "Failed to read entityList pointer (wrong dwEntityList offset?)\n";
        CloseHandle(hProcess);
        return 1;
    }

    uintptr_t listEntry = 0;
    if (!ReadMemory(entityList + ENTITY_ENTRY_OFFSET, listEntry) || listEntry == 0) {
        std::cout << "First listEntry invalid.\n";
        CloseHandle(hProcess);
        return 1;
    }

    // ──────────────────────────────
    //   Scan & print players
    // ──────────────────────────────
    std::cout << "Scanning players (max 64 slots)...\n";
    std::cout << std::left
              << std::setw(5)  << "Idx"
              << std::setw(24) << "Name"
              << "Health\n"
              << std::string(40, '-') << "\n";

    for (int i = 1; i < 64; ++i) {  // skip 0 (world)
        uintptr_t currentController = 0;
        if (!ReadMemory(listEntry + CHUNK_STRIDE * i, currentController) || currentController == 0) {
            continue;
        }

        uint32_t pawnHandle = 0;
        if (!ReadMemory(currentController + m_hPlayerPawn, pawnHandle) ||
            pawnHandle == 0 || pawnHandle == 0xFFFFFFFF) {
            continue;
        }

        // Resolve pawn from handle
        int pawnIndex = pawnHandle & 0x7FFF;
        int listIdx   = pawnIndex >> 9;
        int innerIdx  = pawnIndex & 0x1FF;

        uintptr_t listEntry2 = 0;
        if (!ReadMemory(entityList + 0x8 * listIdx + ENTITY_ENTRY_OFFSET, listEntry2) || listEntry2 == 0) {
            continue;
        }

        uintptr_t pawn = 0;
        if (!ReadMemory(listEntry2 + CHUNK_STRIDE * innerIdx, pawn) || pawn == 0) {
            continue;
        }

        uint32_t health = 0;
        if (!ReadMemory(pawn + m_iHealth, health) || health < 1 || health > 100) {
            continue;
        }

        std::string name = ReadString(currentController + m_iszPlayerName);
        if (name.empty() || name.find_first_not_of(" \t\n\r") == std::string::npos) {
            continue;
        }

        std::cout << std::left
                  << std::setw(5)  << i
                  << std::setw(24) << name
                  << health << " HP\n";
    }

    CloseHandle(hProcess);
    std::cout << "\nDone.\n";

    system("pause");  // optional – keep console open
    return 0;
}