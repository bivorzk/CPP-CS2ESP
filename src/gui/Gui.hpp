#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>

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

        // Legacy compatibility for older UI states. New aimbot mode is controlled by aimbotEnabled/aimbotMode.
        bool     autoAim;
        bool     altAutoFire;

        int      espMode;       // 0=Advanced Chams, 1=Bone ESP, 2=Perfect Flat Chams
        int      espStrength;   // shared line thickness / glow intensity setting
        float    aimSensitivity; // additional aim input scale for mouse movement
        float    aimSmoothing;  // existing smoothing value (legacy)
        int      aimMaxFov;     // maximum dynamic FOV degrees used for target selection
        int      aimPart;        // 0=head, 1=body, 2=arms, 3=legs

        bool     strictVisibility;  // enforce strict locally-visible criteria
        int      visCooldownFrames; // number of frames to confirm new target / keep old after lost

        bool     autoBhop;          // enable bunnyhop automation

        // New aimbot configuration
        bool     aimbotEnabled;
        int      aimbotMode;        // 0=Off, 1=Legit, 2=Rage, 3=Triggerbot-only
        int      targetPriority;    // 0=Crosshair,1=Distance,2=LowHP,3=VisibleOnly,4=FOV
        bool     visibleOnly;
        int      maxDistance;       // meters
        bool     ignoreTeammates;
        bool     ignoreFlashed;
        bool     ignoreSmoke;
        bool     ignoreScopedOnly;
        int      targetSwitchMs;

        float    smoothness;        // 0.0 = instant snap, 1.0 = legit slow
        int      accelCurve;        // 0=Linear,1=Ease-in,2=Ease-out,3=Sigmoid
        float    accelStrength;     // curve strength
        float    randomization;     // random offset strength
        bool     recoilCompensation;
        float    recoilStrength;
        bool     silentAim;
        bool     autoShoot;
        int      minHitChance;      // 0–100
        bool     autoScope;
        bool     autoStop;

        int      aimbotUpdateMs;    // 0=every tick, >0 custom ms
        bool     predictionEnabled;
        bool     resolverEnabled;
        int      resolverStrength;  // 0–100
        bool     visibilityRayTrace;
        int      minReactionMs;

        bool     drawFovCircle;
        COLORREF fovCircleColor;
        int      fovCircleThickness;
        bool     drawTargetLine;
        bool     drawTargetDot;
        bool     drawAimPoint;
        bool     showTargetInfo;

        std::array<int, 7> bonePriorityOrder; // category indices in preferred order
        std::array<bool, 7> bonePriorityEnabled;
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
    void         setVisuals(const VisualConfig& cfg);
    void         setAimbotEnabled(bool enabled);

} // namespace Gui