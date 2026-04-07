#pragma once
// ============================================================
//  gui_state.hpp — Internal shared state
//  Included by gui.cpp, gui_draw.cpp, and all tab files.
//  NEVER include outside the gui_* translation units.
// ============================================================
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include "gui.hpp"

namespace Gui {

// ============================================================
//  Window layout constants
// ============================================================
static constexpr int WIN_W     = 820;
static constexpr int WIN_H     = 680;
static constexpr int SIDEBAR_W = 132;
static constexpr int TAB_H     = 42;
static constexpr int TAB_COUNT = 9;
static constexpr int ID_TAB_BASE = 700;  // sidebar button IDs: 700–708

// ============================================================
//  Control IDs
// ============================================================
enum CtrlID : int {
    ID_CFG_PROCESS      = 100, ID_CFG_POLLMS, ID_CFG_APPLY, ID_CFG_STATUS,
    ID_VIS_WIDTH        = 200, ID_VIS_HEIGHT,  ID_VIS_BORDER, ID_VIS_APPLY,
    ID_VIS_APPLY_MOVEMENT, ID_VIS_APPLY_ESP, ID_VIS_CENTER, ID_VIS_OPACITY, ID_VIS_OPACITY_LBL,
    ID_VIS_R,                  ID_VIS_R_LBL,   ID_VIS_G, ID_VIS_G_LBL,
    ID_VIS_B,                  ID_VIS_B_LBL,
    ID_VIS_SHOW_TEAM_A,
    ID_VIS_AUTO_AIM, ID_VIS_ALT_FIRE, ID_VIS_STRICT_VIS, ID_VIS_ESP_MODE, ID_VIS_ESP_STRENGTH, ID_VIS_ESP_STRENGTH_LBL, ID_VIS_AIM_SENS, ID_VIS_AIM_SMOOTH, ID_VIS_MAX_FOV, ID_VIS_AUTO_BHOP, ID_VIS_AIM_PART, ID_VIS_COOLDOWN,
    ID_AIM_ENABLED, ID_AIM_MODE, ID_AIM_TARGET_PRIORITY, ID_AIM_VISIBLE_ONLY, ID_AIM_MAX_DIST, ID_AIM_IGNORE_TEAM, ID_AIM_IGNORE_FLASH, ID_AIM_IGNORE_SMOKE, ID_AIM_IGNORE_SCOPED, ID_AIM_SWITCH_DELAY,
    ID_AIM_SMOOTHNESS, ID_AIM_ACCEL_CURVE, ID_AIM_ACCEL_STRENGTH, ID_AIM_RANDOMIZATION,
    ID_AIM_RECOIL, ID_AIM_RECOIL_STRENGTH, ID_AIM_SILENT, ID_AIM_AUTO_SHOOT, ID_AIM_MIN_HITCHANCE, ID_AIM_AUTO_SCOPE, ID_AIM_AUTO_STOP,
    ID_AIM_UPDATE_RATE, ID_AIM_PREDICTION, ID_AIM_RESOLVER, ID_AIM_RESOLVER_STRENGTH, ID_AIM_VISIBILITY_CHECK, ID_AIM_MIN_REACT,
    ID_AIM_DRAW_FOV, ID_AIM_FOV_COLOR, ID_AIM_FOV_THICKNESS, ID_AIM_DRAW_LINE, ID_AIM_DRAW_DOT, ID_AIM_DRAW_POINT, ID_AIM_SHOW_INFO,
    ID_AIM_PRESET_LEGIT, ID_AIM_PRESET_RAGE, ID_AIM_PRESET_SAVE,
    ID_AIM_BONE_LIST, ID_AIM_BONE_UP, ID_AIM_BONE_DOWN, ID_AIM_BONE_ENABLE_BASE,
    ID_AIM_TAB_GENERAL, ID_AIM_TAB_BEHAVIOR, ID_AIM_TAB_PERFORMANCE, ID_AIM_TAB_VISUAL,
    ID_MEM_LIST         = 300, ID_MEM_CURVAL,  ID_MEM_WRITEVAL, ID_MEM_WRITEBTN,
    ID_HK_QUIT          = 400, ID_HK_TOGGLE,
    ID_DBG_LOG          = 500, ID_DBG_CLEAR,
    ID_PLAYERS_LIST     = 600,
};

// ============================================================
//  Shared state (defined in gui_draw.cpp / gui.cpp)
// ============================================================
extern HINSTANCE s_hInst;
extern HWND      s_hwnd;
extern HWND      s_panels[TAB_COUNT];
extern HWND      s_tabBtns[TAB_COUNT];
extern int       s_activeTab;

extern Config        s_config;
extern VisualConfig  s_visuals;

extern const char*   TAB_NAMES[TAB_COUNT];

// GDI resources (defined in gui_draw.cpp)
extern HBRUSH s_brDark;
extern HBRUSH s_brPanel;
extern HBRUSH s_brCtrl;
extern HFONT  s_fUI;
extern HFONT  s_fBold;
extern HFONT  s_fMono;
extern HFONT  s_fApp;

} // namespace Gui
