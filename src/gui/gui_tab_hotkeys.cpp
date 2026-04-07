#include "gui_tabs.hpp"
#include "gui_draw.hpp"

namespace Gui {

void buildHotkeys(HWND p) {
    int x=18, y=18;

    mkHeader(p, "KEYBINDINGS", x, y); y+=26;

    struct { const char* lbl; const char* key; int id; } ks[] = {
        { "Quit overlay",   "END",    ID_HK_QUIT   },
        { "Toggle overlay", "INSERT", ID_HK_TOGGLE },
        { "Toggle aimbot",  "F1",     0           },
    };
    for (auto& k : ks) {
        mkLabel(p, k.lbl,  x,       y, 110, 18);
        mkEdit (p, k.key,  x+118,   y,  80, 22, k.id, true);
        y+=30;
    }

    y+=20;
    mkHeader(p, "NOTE", x, y); y+=24;
    mkLabel(p, "Edit RegisterHotKey() in main.cpp and rebuild.", x, y, 340, 18);
}

bool handleHotkeys(WPARAM) {
    return false; // no interactive controls on this tab yet
}

} // namespace Gui
