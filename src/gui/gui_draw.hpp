#pragma once
// ============================================================
//  gui_draw.hpp — Colour palette, GDI resource management,
//                 control factories, paint helpers, PanelProc
// ============================================================
#include "gui_state.hpp"

namespace Gui {

// ============================================================
//  Colour palette
// ============================================================
static constexpr COLORREF C_BG_DARK   = RGB( 14,  17,  24);
static constexpr COLORREF C_BG_PANEL  = RGB( 24,  28,  36);
static constexpr COLORREF C_BG_CTRL   = RGB( 34,  38,  48);
static constexpr COLORREF C_BG_PRESS  = RGB(  0, 112, 184);
static constexpr COLORREF C_ACCENT    = RGB(  90, 200, 255);
static constexpr COLORREF C_ACCENT2   = RGB(  36,  76, 108);
static constexpr COLORREF C_TEXT_PRI  = RGB(235, 239, 247);
static constexpr COLORREF C_TEXT_SEC  = RGB(152, 163, 176);
static constexpr COLORREF C_BORDER    = RGB( 56,  68,  84);

// ============================================================
//  GDI resource lifecycle
// ============================================================
void initRes();
void freeRes();

// ============================================================
//  Control factories
//  All controls automatically receive the correct dark theme
//  font and are parented to `p`.
// ============================================================
HWND mkLabel  (HWND p, const char* t, int x, int y, int w, int h);
HWND mkHeader (HWND p, const char* t, int x, int y);           // accent-coloured bold
HWND mkEdit   (HWND p, const char* t, int x, int y, int w, int h,
               int id, bool ro=false, bool multi=false);
HWND mkButton (HWND p, const char* t, int x, int y, int w, int h, int id);
HWND mkSlider (HWND p, int x, int y, int w, int h,
               int id, int lo, int hi, int val);
HWND mkValLbl (HWND p, int val, int x, int y, int id);

// ============================================================
//  Paint helpers
// ============================================================
void fillRC    (HDC dc, RECT rc, COLORREF c);
void hLine     (HDC dc, int x1, int x2, int y,  COLORREF c);
void vLine     (HDC dc, int x,  int y1, int y2, COLORREF c);
void outlineRC (HDC dc, RECT rc, COLORREF c);

void paintButton (LPDRAWITEMSTRUCT di);
void paintTabBtn (LPDRAWITEMSTRUCT di, int idx);

// ============================================================
//  Helpers used by tab files
// ============================================================
void syncSlider(HWND panel, int sliderId, int labelId);

// ============================================================
//  Panel WndProc (shared across all tab panels)
// ============================================================
LRESULT CALLBACK PanelProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp);

} // namespace Gui
