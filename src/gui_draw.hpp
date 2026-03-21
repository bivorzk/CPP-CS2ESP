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
static constexpr COLORREF C_BG_DARK   = RGB( 10,  10,  14);
static constexpr COLORREF C_BG_PANEL  = RGB( 16,  16,  22);
static constexpr COLORREF C_BG_CTRL   = RGB( 24,  24,  34);
static constexpr COLORREF C_BG_PRESS  = RGB(  0,  65,  82);
static constexpr COLORREF C_ACCENT    = RGB(  0, 210, 255);
static constexpr COLORREF C_ACCENT2   = RGB(  0,  80, 100);
static constexpr COLORREF C_TEXT_PRI  = RGB(220, 220, 238);
static constexpr COLORREF C_TEXT_SEC  = RGB(110, 110, 140);
static constexpr COLORREF C_BORDER    = RGB( 36,  36,  52);

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
