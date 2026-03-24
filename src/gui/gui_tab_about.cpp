#include "gui_tabs.hpp"
#include "gui_draw.hpp"

namespace Gui {

void buildAbout(HWND p) {
    int x=18, y=18;

    mkHeader(p, "OVERLAY TOOLKIT  v1.0", x, y); y+=28;

    auto lbl = [&](const char* t){ mkLabel(p,t,x,y,380,18); y+=20; };
    auto hdr = [&](const char* t){ mkHeader(p,t,x,y); y+=24; };
    auto gap = [&](){ y+=8; };

    lbl("C++20  |  Win32  |  MinGW / MSVC"); gap();
    hdr("MODULES");
    lbl("  mem.hpp          Typed process memory API");
    lbl("  overlay.hpp      Transparent overlay window");
    lbl("  gui.hpp          Control panel (this window)");
    lbl("  offsets.hpp      Game-specific addresses");
    lbl("  gui_draw.hpp     Colours, fonts, primitives");
    lbl("  gui_tab_*.cpp    One file per tab");
    gap();
    hdr("USAGE");
    lbl("Press END at any time to quit cleanly.");
}

} // namespace Gui
