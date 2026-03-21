#include "gui_tabs.hpp"
#include "gui_draw.hpp"

namespace Gui {

void buildMemory(HWND p) {
    int x=18, y=18;

    mkHeader(p, "OFFSETS  (offsets.hpp)", x, y); y+=24;

    HWND lb = CreateWindowA("LISTBOX", "",
        WS_CHILD|WS_VISIBLE|WS_BORDER|LBS_NOTIFY|WS_VSCROLL,
        x, y, 330, 88, p, (HMENU)ID_MEM_LIST, s_hInst, nullptr);
    SendMessage(lb, WM_SETFONT, (WPARAM)s_fMono, TRUE);
    SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)"[Health]  STATIC_PTR + chain   float");
    SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)"[PosX]    STATIC_PTR + chain   float");
    SendMessageA(lb, LB_ADDSTRING, 0, (LPARAM)"[Ammo]    STATIC_PTR + chain   int  ");
    y+=100;

    mkHeader(p, "READ / WRITE", x, y); y+=24;
    mkLabel(p, "Current value", x, y, 90, 16); y+=17;
    mkEdit (p, "—", x, y, 160, 24, ID_MEM_CURVAL, true);
    y+=34;
    mkLabel(p, "Write value", x, y, 80, 16); y+=17;
    mkEdit  (p, "",        x,     y, 130, 24, ID_MEM_WRITEVAL);
    mkButton(p, "Write",   x+138, y,  72, 26, ID_MEM_WRITEBTN);
}

bool handleMemory(WPARAM wp) {
    if (LOWORD(wp) != ID_MEM_WRITEBTN) return false;
    // Wire this up in main.cpp game logic
    return true;
}

} // namespace Gui
