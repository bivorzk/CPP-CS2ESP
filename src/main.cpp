#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <array>
#include <thread>
#include <atomic>

#include "mem.hpp"
#include "overlay.hpp"
#include "gui.hpp"
#include "offsets.hpp"
#include "player_scanner.hpp"
#include "bomb_found.hpp"
#include "aim.hpp"
#include "game/bhop/bhop.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "comctl32.lib")

// ============================================================
//  Globals
// ============================================================
static mem::ProcessMemory* g_proc = nullptr;
static std::atomic<bool> g_polling = false;
static std::thread g_pollThread;

// ============================================================
//  Run one scan+aim cycle (reusable for timer and fast thread)
// ============================================================
static void runTick() {
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

    static float lastBombRemain = 0.0f;
    static uint64_t lastBombSeenMs = 0;

    if (running) {
        Overlay::show();

        Bomb::Info bombInfo = Bomb::Finder::read(g_proc);
        bool changed = PlayerScanner::scanPlayers(g_proc, &bombInfo);
        if (changed) {
            // silent (no frequent log) for speed
        }

        uint64_t nowMs = GetTickCount64();

        if (bombInfo.valid && bombInfo.blowTime > 0.0f) {
            lastBombRemain = bombInfo.blowTime;
            lastBombSeenMs = nowMs;
            Gui::updateBombTimer(lastBombRemain, true);
        } else if (nowMs - lastBombSeenMs <= 500 && lastBombRemain > 0.0f) {
            Gui::updateBombTimer(lastBombRemain, true);
        } else {
            lastBombRemain = 0.0f;
            Gui::updateBombTimer(0.0f, false);
        }

        Aim::update(g_proc);
        BHop::update(g_proc);
    } else {
        Overlay::hide();
    }
}

// ============================================================
//  Poll timer — fires every Config::pollMs milliseconds
// ============================================================
VOID CALLBACK pollTimer(HWND, UINT, UINT_PTR, DWORD) {
    runTick();
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
    runTick();

    // Start dedicated high-priority scan thread. Use configured poll interval (min 10ms).
    g_polling = true;
    g_pollThread = std::thread([]() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        while (g_polling) {
            runTick();
            DWORD pollMs = Gui::getConfig().pollMs;
            if (pollMs < 10) pollMs = 10;
            Sleep(pollMs);
        }
    });

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

    g_polling = false;
    if (g_pollThread.joinable()) g_pollThread.join();
    UnregisterHotKey(nullptr, 1);
    Overlay::destroy();
    Gui::destroy();
    delete g_proc;
    return 0;
}