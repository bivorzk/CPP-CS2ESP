#pragma once
// ============================================================
//  gui_tabs.hpp — build + handle declarations for every tab
// ============================================================
#include "gui_state.hpp"

namespace Gui {

    void buildConfig  (HWND p);  bool handleConfig  (WPARAM wp);
    void buildCombat  (HWND p);  bool handleCombat  (WPARAM wp);
    void buildMovement(HWND p);  bool handleMovement(WPARAM wp);
    void buildMemory  (HWND p);  bool handleMemory  (WPARAM wp);
    void buildHotkeys (HWND p);  bool handleHotkeys (WPARAM wp);
    void buildDebug   (HWND p);  bool handleDebug   (WPARAM wp);
    void buildAbout   (HWND p);
    void buildPlayers (HWND p);  bool handlePlayers (WPARAM wp);
    void clearPlayers();

    // Call from main.cpp game logic to push live data into the Players tab
    void updatePlayer(int team, int slot,
                      const char* name, int hp, int hpMax, bool alive);

} // namespace Gui
