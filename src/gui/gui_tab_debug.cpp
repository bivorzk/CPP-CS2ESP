#include "gui_tabs.hpp"
#include "gui_draw.hpp"

namespace Gui {

void buildDebug(HWND p) {
    RECT rc; GetClientRect(p, &rc);
    int x=18, y=18;

    mkHeader(p, "LOG OUTPUT", x, y); y+=24;
    int logH = rc.bottom - y - 36;
    if (logH < 60) logH = 60;
    mkEdit  (p, "", x, y, rc.right-x-18, logH, ID_DBG_LOG, true, true);
    y += logH + 8;
    mkButton(p, "Clear", x, y, 72, 24, ID_DBG_CLEAR);
}

bool handleDebug(WPARAM wp) {
    if (LOWORD(wp) != ID_DBG_CLEAR) return false;
    SetWindowTextA(GetDlgItem(s_panels[4], ID_DBG_LOG), "");
    return true;
}

} // namespace Gui
