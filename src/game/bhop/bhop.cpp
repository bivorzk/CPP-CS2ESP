#include "bhop.hpp"
#include "gui.hpp"
#include "offsets.hpp"
#include <windows.h>

namespace BHop {

static bool s_wasOnGround = false;

void update(mem::ProcessMemory* proc) {
    if (!proc) return;

    Gui::VisualConfig visuals = Gui::getVisuals();
    if (!visuals.autoBhop) return;

    uintptr_t base = proc->getModuleBase(Offsets::MODULE);
    if (!base) return;

    auto localPawnOpt = proc->read<uintptr_t>(base + Offsets::dwLocalPlayerPawn::STATIC_PTR);
    if (!localPawnOpt || *localPawnOpt == 0) return;

    uintptr_t localPawn = *localPawnOpt;
    auto flagsOpt = proc->read<int32_t>(localPawn + Offsets::m_fFlags::STATIC_PTR);
    if (!flagsOpt) return;

    const int FL_ONGROUND = 1;
    bool onGround = ((*flagsOpt & FL_ONGROUND) != 0);
    bool spaceHeld = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

    // Bunnyhop behavior: while SPACE is held and we are on ground, trigger jump each tick.
    if (spaceHeld && onGround) {
        INPUT input[2]{};
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_SPACE;
        input[0].ki.dwFlags = 0;

        input[1].type = INPUT_KEYBOARD;
        input[1].ki.wVk = VK_SPACE;
        input[1].ki.dwFlags = KEYEVENTF_KEYUP;

        SendInput(2, input, sizeof(INPUT));
    }

    // Keep track to avoid false positive on-ground transitions being relied on.
    s_wasOnGround = onGround;
}

} // namespace BHop
