#include "gui_tabs.hpp"
#include "gui_draw.hpp"
#include <cstdio>
#include <cstdlib>

namespace Gui {

void buildConfig(HWND p) {
    int x=18, y=18;

    mkHeader(p, "TARGET PROCESS", x, y); y+=26;
    mkLabel (p, "Process name",   x,      y, 96, 16);
    mkLabel (p, "Poll  (ms)",     x+224,  y, 76, 16);
    y+=17;

    mkEdit(p, s_config.targetProcess, x,     y, 210, 24, ID_CFG_PROCESS);
    char buf[16]; sprintf_s(buf, "%lu", s_config.pollMs);
    mkEdit(p, buf,                    x+220, y,  70, 24, ID_CFG_POLLMS);
    y+=34;

    mkButton(p, "Apply Config", x, y, 102, 26, ID_CFG_APPLY);
    y+=46;

    mkHeader(p, "STATUS", x, y); y+=24;
    HWND st = CreateWindowA("STATIC", "Searching...",
        WS_CHILD|WS_VISIBLE|SS_LEFT,
        x, y, 300, 20, p, (HMENU)ID_CFG_STATUS, s_hInst, nullptr);
    SendMessage(st, WM_SETFONT, (WPARAM)s_fMono, TRUE);
}

// Returns true if the ID was handled here
bool handleConfig(WPARAM wp) {
    if (LOWORD(wp) != ID_CFG_APPLY) return false;

    HWND p = s_panels[0];
    GetWindowTextA(GetDlgItem(p, ID_CFG_PROCESS), s_config.targetProcess, 64);
    char buf[16];
    GetWindowTextA(GetDlgItem(p, ID_CFG_POLLMS), buf, 16);
    s_config.pollMs = (DWORD)atoi(buf);
    return true;
}

} // namespace Gui
