#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <array>

#include "mem.hpp"
#include "overlay.hpp"
#include "gui.hpp"
#include "offsets.hpp"
#include "player_scanner.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "comctl32.lib")

// ============================================================
//  Globals
// ============================================================
static mem::ProcessMemory* g_proc = nullptr;

// ============================================================
//  Poll timer — fires every Config::pollMs milliseconds
// ============================================================
VOID CALLBACK pollTimer(HWND, UINT, UINT_PTR, DWORD) {
    Gui::Config cfg = Gui::getConfig();

    // Check if existing handle is still alive
    bool running = false;
    if (g_proc) {
        DWORD exitCode = 0;
        running = GetExitCodeProcess(g_proc->handle(), &exitCode)
                  && exitCode == STILL_ACTIVE;
        if (!running) {
            Gui::log("[*] %s exited — handle released.", cfg.targetProcess);
            Gui::setStatus("Searching...");
            delete g_proc;
            g_proc = nullptr;
        }
    }

    // Try to attach if we don't have a live handle
    if (!running) {
        try {
            g_proc  = new mem::ProcessMemory(cfg.targetProcess);
            running = true;
            char status[80];
            sprintf_s(status, "Attached (PID %lu)", g_proc->pid());
            Gui::log("[+] Attached to %s (PID %lu)", cfg.targetProcess, g_proc->pid());
            Gui::setStatus(status);
        } catch (const std::exception& ex) {
            Gui::log("[-] Attach failed (%s): %s", cfg.targetProcess, ex.what());
            Gui::setStatus("Attach failed");
        } catch (...) {
            Gui::log("[-] Attach failed (%s): unknown error", cfg.targetProcess);
            Gui::setStatus("Attach failed");
        }
    }

    if (running) {
        Overlay::show();

        // ====================================================
        //  YOUR GAME LOGIC HERE
        //  g_proc is a valid live ProcessMemory* at this point.

        bool changed = PlayerScanner::scanPlayers(g_proc);
        if (changed) {
            Gui::log("[*] Event: scan result changed");
        }
        //
        //  -- Module base --
        //  uintptr_t base = g_proc->getModuleBase(Offsets::MODULE);
        //
        //  -- Read via pointer chain --
        //  uintptr_t addr = g_proc->resolvePointerChain(
        //      base + Offsets::Health::STATIC_PTR,
        //      Offsets::Health::CHAIN
        //  );
        //  auto hp = g_proc->read<float>(addr);
        //  if (hp) Gui::log("Health: %.1f", *hp);
        //
        //  -- Write --
        //  g_proc->write<float>(addr, 9999.f);
        //
        //  -- Pattern scan --
        //  uintptr_t hit = g_proc->patternScan(
        //      base, 0x1000000, "89 87 ? ? 00 00 8B 47 10");
        //  if (hit) Gui::log("Pattern @ 0x%llX", (unsigned long long)hit);
        // ====================================================

    } else {
        Overlay::hide();
    }
}

// ============================================================
//  Entry point
// ============================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {

    // Open console (Debug tab mirrors output here too)
    AllocConsole();
    FILE* dummy;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    SetConsoleTitleA("Overlay Console");

    // Init GUI first — Overlay::create() reads VisualConfig from it
    if (!Gui::init(hInst)) return 1;
    Gui::show();
    Gui::log("[*] Overlay started. Watching for: %s", Gui::getConfig().targetProcess);
    Gui::log("[*] Press END to quit.");

    if (!Overlay::create(hInst)) {
        Gui::log("[-] Failed to create overlay window.");
        return 1;
    }

    // END key quits cleanly from anywhere
    RegisterHotKey(nullptr, 1, MOD_NOREPEAT, VK_END);

    // First tick immediately — no cold-start delay
    pollTimer(nullptr, 0, 0, 0);

    // Use very aggressive refresh for responsive ESP; configured in GUI but hardcoded for quality
    const UINT refreshRateMs = 8;
    SetTimer(nullptr, 1, refreshRateMs, pollTimer);

    // Single message loop handles all windows + timer in this thread
    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY && msg.wParam == 1) {
            Gui::log("[*] END pressed — shutting down.");
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    KillTimer(nullptr, 1);
    UnregisterHotKey(nullptr, 1);
    Overlay::destroy();
    Gui::destroy();
    delete g_proc;
    return 0;
}