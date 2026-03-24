#pragma once

#include <windows.h>
#include <atomic>
#include <memory>
#include <thread>

namespace mem {
    class ProcessMemory;
}

class App {
public:
    App();
    ~App();

    bool init(HINSTANCE hInst);
    int run(HINSTANCE hInst);
    void stop();

private:
    void runTick();
    void startPolling();
    void stopPolling();

    static VOID CALLBACK pollTimer(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);
    static App* s_instance;

    std::unique_ptr<mem::ProcessMemory> proc_;
    std::atomic<bool> polling_;
    std::thread pollThread_;

    float lastBombRemain_;
    uint64_t lastBombSeenMs_;
};
