#include "gui_draw.hpp"
#include <cstdio>

namespace Gui {

// ============================================================
//  Definitions of shared state declared in gui_state.hpp
// ============================================================
HINSTANCE s_hInst    = nullptr;
HWND      s_hwnd     = nullptr;
HWND      s_panels[TAB_COUNT]  = {};
HWND      s_tabBtns[TAB_COUNT] = {};
int       s_activeTab = 0;

Config      s_config  = { "cs2.exe", 8 };   // reduced poll interval for near-realtime updates
VisualConfig s_visuals = { 300, 200, 3, 255, RGB(255,255,255), 0, 0,
                           false, false, false, 0, false, 0, false };

const char* TAB_NAMES[TAB_COUNT] = {
    "Config", "Combat", "Movement", "Memory", "Hotkeys", "Debug", "About", "Players"
};

HBRUSH s_brDark  = nullptr;
HBRUSH s_brPanel = nullptr;
HBRUSH s_brCtrl  = nullptr;
HFONT  s_fUI     = nullptr;
HFONT  s_fBold   = nullptr;
HFONT  s_fMono   = nullptr;
HFONT  s_fApp    = nullptr;

// ============================================================
//  GDI resource lifecycle
// ============================================================
static HFONT makeFont(int pt, bool bold, bool mono) {
    HDC dc = GetDC(nullptr);
    int h  = -MulDiv(pt, GetDeviceCaps(dc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, dc);
    return CreateFontA(h,0,0,0,
        bold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE,FALSE,FALSE,
        DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
        DEFAULT_PITCH|FF_DONTCARE,
        mono ? "Consolas" : "Segoe UI");
}

void initRes() {
    s_brDark  = CreateSolidBrush(C_BG_DARK);
    s_brPanel = CreateSolidBrush(C_BG_PANEL);
    s_brCtrl  = CreateSolidBrush(C_BG_CTRL);
    s_fUI   = makeFont( 9, false, false);
    s_fBold = makeFont( 9, true,  false);
    s_fMono = makeFont( 9, false, true );
    s_fApp  = makeFont(11, true,  false);
}

void freeRes() {
    HGDIOBJ r[] = { s_brDark,s_brPanel,s_brCtrl,s_fUI,s_fBold,s_fMono,s_fApp };
    for (auto h : r) if (h) DeleteObject(h);
}

// ============================================================
//  Control factories
// ============================================================
static void setFont(HWND hw, HFONT f) {
    SendMessage(hw, WM_SETFONT, (WPARAM)f, TRUE);
}

HWND mkLabel(HWND p, const char* t, int x, int y, int w, int h) {
    HWND hw = CreateWindowA("STATIC", t,
        WS_CHILD|WS_VISIBLE|SS_LEFT, x,y,w,h,
        p, nullptr, s_hInst, nullptr);
    setFont(hw, s_fUI);
    return hw;
}

HWND mkHeader(HWND p, const char* t, int x, int y) {
    HWND hw = CreateWindowA("STATIC", t,
        WS_CHILD|WS_VISIBLE|SS_LEFT,
        x, y, 380, 17, p, nullptr, s_hInst, nullptr);
    setFont(hw, s_fBold);
    return hw;
}

HWND mkEdit(HWND p, const char* t, int x, int y, int w, int h,
            int id, bool ro, bool multi)
{
    DWORD st = WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL;
    if (ro)    st |= ES_READONLY;
    if (multi) st |= ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL;
    HWND hw = CreateWindowA("EDIT", t, st, x,y,w,h,
        p, (HMENU)(intptr_t)id, s_hInst, nullptr);
    setFont(hw, s_fMono);
    return hw;
}

HWND mkButton(HWND p, const char* t, int x, int y, int w, int h, int id) {
    HWND hw = CreateWindowA("BUTTON", t,
        WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
        x,y,w,h, p, (HMENU)(intptr_t)id, s_hInst, nullptr);
    setFont(hw, s_fBold);
    return hw;
}

HWND mkSlider(HWND p, int x, int y, int w, int h,
              int id, int lo, int hi, int val)
{
    HWND hw = CreateWindowA(TRACKBAR_CLASSA, "",
        WS_CHILD|WS_VISIBLE|TBS_HORZ|TBS_NOTICKS,
        x,y,w,h, p, (HMENU)(intptr_t)id, s_hInst, nullptr);
    SendMessage(hw, TBM_SETRANGE, TRUE, MAKELPARAM(lo,hi));
    SendMessage(hw, TBM_SETPOS,   TRUE, val);
    return hw;
}

HWND mkValLbl(HWND p, int val, int x, int y, int id) {
    char buf[8]; sprintf_s(buf, "%d", val);
    HWND hw = CreateWindowA("STATIC", buf,
        WS_CHILD|WS_VISIBLE|SS_RIGHT,
        x,y,32,18, p, (HMENU)(intptr_t)id, s_hInst, nullptr);
    setFont(hw, s_fMono);
    return hw;
}

// ============================================================
//  Paint helpers
// ============================================================
void fillRC(HDC dc, RECT rc, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, &rc, b);
    DeleteObject(b);
}

void hLine(HDC dc, int x1, int x2, int y, COLORREF c) {
    HPEN p = CreatePen(PS_SOLID,1,c), o = (HPEN)SelectObject(dc,p);
    MoveToEx(dc,x1,y,nullptr); LineTo(dc,x2,y);
    SelectObject(dc,o); DeleteObject(p);
}

void vLine(HDC dc, int x, int y1, int y2, COLORREF c) {
    HPEN p = CreatePen(PS_SOLID,1,c), o = (HPEN)SelectObject(dc,p);
    MoveToEx(dc,x,y1,nullptr); LineTo(dc,x,y2);
    SelectObject(dc,o); DeleteObject(p);
}

void outlineRC(HDC dc, RECT rc, COLORREF c) {
    HPEN   p  = CreatePen(PS_SOLID,1,c);
    HPEN   op = (HPEN)  SelectObject(dc, p);
    HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH ob = (HBRUSH)SelectObject(dc, nb);
    Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dc, op); DeleteObject(p);
    SelectObject(dc, ob);
}

void paintButton(LPDRAWITEMSTRUCT di) {
    HDC  dc  = di->hDC;
    RECT rc  = di->rcItem;
    bool dn  = (di->itemState & ODS_SELECTED) != 0;
    bool dis = (di->itemState & ODS_DISABLED)  != 0;

    // Modern rounded button style
    HBRUSH bg = CreateSolidBrush(dn ? C_BG_PRESS : C_BG_CTRL);
    HPEN   border = CreatePen(PS_SOLID, 1, dn ? C_ACCENT : C_BORDER);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, bg);
    HPEN oldPen = (HPEN)SelectObject(dc, border);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 12, 12);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(border);
    DeleteObject(bg);

    char txt[80]{}; GetWindowTextA(di->hwndItem, txt, 80);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, dis ? C_TEXT_SEC : (dn ? C_ACCENT : C_TEXT_PRI));
    HFONT of = (HFONT)SelectObject(dc, s_fBold);
    DrawTextA(dc, txt, -1, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(dc, of);
}

void paintTabBtn(LPDRAWITEMSTRUCT di, int idx) {
    HDC  dc   = di->hDC;
    RECT rc   = di->rcItem;
    bool act  = (idx == s_activeTab);

    // Tab button shadows + modern material style
    COLORREF bgColor   = act ? RGB( 34,  42,  56) : C_BG_DARK;
    COLORREF txtColor  = act ? C_ACCENT : C_TEXT_SEC;
    fillRC(dc, rc, bgColor);
    if (act) {
        RECT bar = { rc.left+8, rc.bottom-3, rc.right-8, rc.bottom-1 };
        fillRC(dc, bar, C_ACCENT);
    }

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, txtColor);
    HFONT of = (HFONT)SelectObject(dc, act ? s_fBold : s_fUI);
    RECT tr  = { rc.left+18, rc.top, rc.right-6, rc.bottom };
    DrawTextA(dc, TAB_NAMES[idx], -1, &tr, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    SelectObject(dc, of);
}

void syncSlider(HWND panel, int slId, int lbId) {
    HWND sl = GetDlgItem(panel, slId);
    HWND lb = GetDlgItem(panel, lbId);
    if (!sl || !lb) return;
    char buf[8]; sprintf_s(buf, "%d", (int)SendMessage(sl, TBM_GETPOS, 0, 0));
    SetWindowTextA(lb, buf);
}

// ============================================================
//  PanelProc — shared WndProc for all tab content panels
//  Routes WM_COMMAND up to the main GuiProc.
// ============================================================
LRESULT CALLBACK PanelProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

        case WM_ERASEBKGND: {
            RECT rc; GetClientRect(hw, &rc);
            FillRect((HDC)wp, &rc, s_brPanel);
            return 1;
        }
        case WM_CTLCOLOREDIT: {
            HDC dc = (HDC)wp;
            SetTextColor(dc, C_TEXT_PRI);
            SetBkColor  (dc, C_BG_CTRL);
            return (LRESULT)s_brCtrl;
        }
        case WM_CTLCOLORLISTBOX: {
            HDC dc = (HDC)wp;
            SetTextColor(dc, C_TEXT_PRI);
            SetBkColor  (dc, C_BG_CTRL);
            return (LRESULT)s_brCtrl;
        }
        case WM_CTLCOLORSTATIC: {
            HDC  dc = (HDC)wp;
            HWND ct = (HWND)lp;
            bool isHdr = ((HFONT)SendMessage(ct, WM_GETFONT, 0, 0) == s_fBold);
            SetTextColor(dc, isHdr ? C_ACCENT : C_TEXT_PRI);
            SetBkColor  (dc, C_BG_PANEL);
            return (LRESULT)s_brPanel;
        }
        case WM_DRAWITEM: {
            paintButton((LPDRAWITEMSTRUCT)lp);
            return TRUE;
        }
        case WM_HSCROLL: {
            HWND sl = (HWND)lp;
            switch (GetDlgCtrlID(sl)) {
                case ID_VIS_OPACITY: syncSlider(hw,ID_VIS_OPACITY,ID_VIS_OPACITY_LBL); break;
                case ID_VIS_R:       syncSlider(hw,ID_VIS_R,      ID_VIS_R_LBL);       break;
                case ID_VIS_G:       syncSlider(hw,ID_VIS_G,      ID_VIS_G_LBL);       break;
                case ID_VIS_B:       syncSlider(hw,ID_VIS_B,      ID_VIS_B_LBL);       break;
            }
            break;
        }
        case WM_COMMAND:
            SendMessage(GetParent(hw), WM_COMMAND, wp, lp);
            break;
    }
    return DefWindowProcA(hw, msg, wp, lp);
}

} // namespace Gui
