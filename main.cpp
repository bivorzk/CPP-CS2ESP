#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <stdexcept>

#include "mem.hpp"
#include "overlay.hpp"
#include "offsets.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "psapi.lib")

// ============================================================
//  Config
// ============================================================
static constexpr const char* TARGET_PROCESS = "chrome.exe";
static constexpr DWORD       POLL_MS        = 500;
// ============================================================

static mem::ProcessMemory* g_proc = nullptr;

// ============================================================
//  Poll timer — runs every POLL_MS milliseconds
// ============================================================
VOID CALLBACK pollTimer(HWND, UINT, UINT_PTR, DWORD) {

    // Check whether the existing handle is still alive
    bool running = false;
    if (g_proc) {
        DWORD exitCode = 0;
        running = GetExitCodeProcess(g_proc->handle(), &exitCode)
                  && exitCode == STILL_ACTIVE;
        if (!running) {
            printf("[*] Process exited — releasing handle.\n");
            delete g_proc;
            g_proc = nullptr;
        }
    }

    // Try to attach if we don't have a live handle yet
    if (!running) {
        try {
            g_proc  = new mem::ProcessMemory(TARGET_PROCESS);
            running = true;
            printf("[+] Attached to %s  (PID %lu)\n",
                   TARGET_PROCESS, g_proc->pid());
        } catch (const std::exception& e) {
            // Process not open — silent until next tick
            (void)e;
        }
    }

    if (running) {
        Overlay::show();

        // ====================================================
        //  YOUR GAME LOGIC HERE
        //  g_proc is a valid live ProcessMemory* at this point.
        //
        //  -- Module base --
        //  uintptr_t base = g_proc->getModuleBase(Offsets::MODULE);
        //
        //  -- Read a value via pointer chain --
        //  uintptr_t addr = g_proc->resolvePointerChain(
        //      base + Offsets::Health::STATIC_PTR,
        //      Offsets::Health::CHAIN
        //  );
        //  auto hp = g_proc->read<float>(addr);
        //  if (hp) printf("Health: %.1f\n", *hp);
        //
        //  -- Write a value --
        //  g_proc->write<float>(addr, 9999.f);
        //
        //  -- Pattern scan --
        //  uintptr_t hit = g_proc->patternScan(
        //      base, 0x1000000,
        //      "89 87 ? ? 00 00 8B 47 10"
        //  );
        //  if (hit) printf("Pattern found at: 0x%llX\n", hit);
        // ====================================================

    } else {
        Overlay::hide();
    }
}

// ============================================================
//  Entry point
// ============================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {

    // Open a console window alongside the overlay
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    SetConsoleTitleA("Overlay Console");
    printf("[*] Overlay started. Watching for %s ...\n", TARGET_PROCESS);
    printf("[*] Press END to quit.\n\n");

    if (!Overlay::create(hInst)) {
        printf("[-] Failed to create overlay window.\n");
        return 1;
    }

    // Global hotkey — press END to exit cleanly
    RegisterHotKey(nullptr, 1, MOD_NOREPEAT, VK_END);

    // Run one tick immediately (no cold-start delay)
    pollTimer(nullptr, 0, 0, 0);

    SetTimer(nullptr, 1, POLL_MS, pollTimer);

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY && msg.wParam == 1) {
            printf("[*] END pressed — shutting down.\n");
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    KillTimer(nullptr, 1);
    UnregisterHotKey(nullptr, 1);
    Overlay::destroy();
    delete g_proc;
    return 0;
}
