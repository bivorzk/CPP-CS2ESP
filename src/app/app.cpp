#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "app.hpp"
#include "overlay.hpp"
#include "gui.hpp"
#include "offsets.hpp"
#include "player_scanner.hpp"
#include "bomb_found.hpp"
#include "aim.hpp"
#include "mem.hpp"

#include <psapi.h>
#include <cstdio>
#include <stdexcept>
#include <string>

App* App::s_instance = nullptr;

App::App()
    : polling_(false)
    , lastBombRemain_(0.0f)
    , lastBombSeenMs_(0)
{
    s_instance = this;
}

App::~App() {
    stop();
}

bool App::init(HINSTANCE hInst) {
    if (!Gui::init(hInst)) {
        return false;
    }

    Gui::show();
    Gui::log("[*] Overlay started. Watching for: %s", Gui::getConfig().targetProcess);
    Gui::log("[*] Press END to quit.");

    if (!Overlay::create(hInst)) {
        Gui::log("[-] Failed to create overlay window.");
        return false;
    }

    RegisterHotKey(nullptr, 1, MOD_NOREPEAT, VK_END);
    return true;
}

int App::run(HINSTANCE hInst) {
    runTick();
    startPolling();

    MSG msg{};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_HOTKEY && msg.wParam == 1) {
            Gui::log("[*] END pressed — shutting down.");
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    stop();
    return 0;
}

void App::stop() {
    stopPolling();
    UnregisterHotKey(nullptr, 1);
    Overlay::destroy();
    Gui::destroy();
    proc_.reset();
}

void App::startPolling() {
    polling_ = true;
    pollThread_ = std::thread([this]() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        while (polling_) {
            runTick();
            DWORD pollMs = Gui::getConfig().pollMs;
            if (pollMs < 10) pollMs = 10;
            Sleep(pollMs);
        }
    });
}

void App::stopPolling() {
    polling_ = false;
    if (pollThread_.joinable()) {
        pollThread_.join();
    }
}

VOID CALLBACK App::pollTimer(HWND, UINT, UINT_PTR, DWORD) {
    if (s_instance) {
        s_instance->runTick();
    }
}

static bool isProcessHandleAlive(mem::ProcessMemory* proc) {
    if (!proc) return false;
    DWORD exitCode = 0;
    return GetExitCodeProcess(proc->handle(), &exitCode) && exitCode == STILL_ACTIVE;
}

void App::runTick() {
    Gui::Config cfg = Gui::getConfig();
    bool running = isProcessHandleAlive(proc_.get());

    if (!running) {
        proc_.reset();
    }

    if (!running) {
        try {
            proc_ = std::make_unique<mem::ProcessMemory>(cfg.targetProcess);
            running = true;
            char status[80];
            sprintf_s(status, "Attached (PID %lu)", proc_->pid());
            Gui::log("[+] Attached to %s (PID %lu)", cfg.targetProcess, proc_->pid());
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

        auto bombInfo = Bomb::Finder::read(proc_.get());
        bool changed = PlayerScanner::scanPlayers(proc_.get(), &bombInfo);
        (void)changed;

        uint64_t nowMs = GetTickCount64();

        if (bombInfo.valid && bombInfo.blowTime > 0.0f) {
            lastBombRemain_ = bombInfo.blowTime;
            lastBombSeenMs_ = nowMs;
            Gui::updateBombTimer(lastBombRemain_, true);
        } else if (nowMs - lastBombSeenMs_ <= 500 && lastBombRemain_ > 0.0f) {
            Gui::updateBombTimer(lastBombRemain_, true);
        } else {
            lastBombRemain_ = 0.0f;
            Gui::updateBombTimer(0.0f, false);
        }

        Aim::update(proc_.get());
    } else {
        Overlay::hide();
    }
}
