# CS2 Aimbot + ESP Overlay

This repository contains a Counter-Strike 2 external overlay and aimbot proof-of-concept.

## Features

- Player scan from process memory
- ESP bounding boxes and bomb markers with corner-style outlines
- Strict visibility checks (dormant/spotted + distance/cone filtering)
- Config UI: overlay size/color, auto-aim, strict visibility, target hold frames
- Aimbot with recoil control and target stickiness
- Foreground-window gating (CS2 only)

## Build

Windows (MSYS2 / MinGW):

```sh
cd C:\Users\bivorzk\projects\CPP-CS2ESP
g++ src/main.cpp src/app/app.cpp src/aim/aim.cpp src/aim/aim_aux.cpp src/overlay/overlay.cpp src/gui/gui.cpp src/gui/gui_draw.cpp src/gui/gui_tab_config.cpp src/gui/gui_tab_debug.cpp src/gui/gui_tab_hotkeys.cpp src/gui/gui_tab_memory.cpp src/gui/gui_tab_visuals.cpp src/gui/gui_tab_about.cpp src/gui/gui_tab_players.cpp src/game/player_scan/player_scanner.cpp src/game/player_scan/player_scanner_helpers.cpp src/game/bomb/bomb_found.cpp -o untitled15.exe -static -std=c++23 -mwindows -I. -Isrc -Isrc/app -Isrc/aim -Isrc/gui -Isrc/overlay -Isrc/game -Isrc/game/player_scan -Isrc/game/bomb -luser32 -lgdi32 -lopengl32 -lcomctl32 -ldwmapi -DDEBUG
```

## Usage

1. Run `untitled15.exe` (as administrator if needed).
2. Make sure CS2 is running and foreground.
3. Configure in GUI:
   - process name
   - poll rate (default 8ms)
   - overlay options
   - auto-aim/strict visibility
4. Use `END` key to exit.

## Notes

- This code is for learning/educational use only.
- Use it responsibly and respect game policies and local law.
