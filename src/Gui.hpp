#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// ============================================================
//  gui.hpp — Win32 tabbed control panel
//
//  Tabs: Config | Visuals | Memory | Hotkeys | Debug | About
//
//  Call order in WinMain:
//    Gui::init(hInst)   — create window (hidden)
//    Gui::show()        — make it visible
//    Gui::log("...")    — append a line to the Debug tab + console
//    Gui::setStatus("") — update the status bar in Config tab
//
//  Read back user settings any time:
//    Gui::getConfig()       → Config struct
//    Gui::getVisuals()      → VisualConfig struct
// ============================================================

namespace Gui {

    // -- Shared config structs (read by main.cpp) ------------

    struct Config {
        char  targetProcess[64];
        DWORD pollMs;
    };

    struct VisualConfig {
        int      width, height;
        int      borderPx;
        int      opacity;       // 0–255
        COLORREF color;
        int      posX, posY;
        bool     showTeamABoxes; // draw friendly team boxes in overlay (Team A)
        bool     autoAim;        // enable auto aim+shoot without key hold

        bool     strictVisibility;  // enforce strict locally-visible criteria
        int      visCooldownFrames; // number of frames to confirm new target / keep old after lost
    };

    // -- Lifecycle -------------------------------------------
    bool  init(HINSTANCE hInst);
    void  show();
    void  destroy();
    HWND  hwnd();

    // -- Runtime API -----------------------------------------
    void  log(const char* fmt, ...);        // Debug tab + console
    void  setStatus(const char* text);      // Config tab status bar
    void  updateBombTimer(float secondsRemaining, bool active); // Players tab

    // -- Config reads ----------------------------------------
    Config       getConfig();
    VisualConfig getVisuals();

} // namespace Gui