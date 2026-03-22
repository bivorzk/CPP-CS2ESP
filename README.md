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
cd C:\Users\bivorzk\CLionProjects\untitled15
g++ src/main.cpp src/overlay.cpp src/gui.cpp src/gui_draw.cpp src/gui_tab_config.cpp src/gui_tab_debug.cpp src/gui_tab_hotkeys.cpp src/gui_tab_memory.cpp src/gui_tab_visuals.cpp src/gui_tab_about.cpp src/gui_tab_players.cpp src/player_scanner.cpp src/bomb_found.cpp src/aim.cpp -o untitled15.exe -static -std=c++23 -mwindows -I. -luser32 -lgdi32 -lopengl32 -lcomctl32 -ldwmapi -DDEBUG
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
