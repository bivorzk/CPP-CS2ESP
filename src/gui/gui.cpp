#include "gui_draw.hpp"
#include "gui_tabs.hpp"
#include <dwmapi.h>
#include <cstdarg>
#include <cstdio>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

namespace Gui {

// ============================================================
//  Scroll state (Visuals)
// ============================================================
HWND s_visualsContent = nullptr;
int   s_visualsContentHeight = 0;

// ============================================================
//  Tab selection
// ============================================================
static void selectTab(int idx) {
    s_activeTab = idx;
    for (int i = 0; i < TAB_COUNT; ++i) {
        ShowWindow(s_panels[i], i == idx ? SW_SHOW : SW_HIDE);
        if (s_tabBtns[i]) InvalidateRect(s_tabBtns[i], nullptr, TRUE);
    }
    InvalidateRect(s_hwnd, nullptr, FALSE);
}

// ============================================================
//  Main window WndProc
// ============================================================
static LRESULT CALLBACK GuiProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

        case WM_ERASEBKGND: {
            RECT rc; GetClientRect(hw, &rc);
            HBRUSH gradient = CreateSolidBrush(C_BG_PANEL);
            FillRect((HDC)wp, &rc, gradient);
            DeleteObject(gradient);
            return 1;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hw, &ps);

            // Sidebar title
            RECT hdr = { 0, 0, SIDEBAR_W, TAB_H };
            fillRC(dc, hdr, C_BG_DARK);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, C_ACCENT);
            HFONT of = (HFONT)SelectObject(dc, s_fApp);
            RECT tr = { 12, 0, SIDEBAR_W, TAB_H };
            DrawTextA(dc, "OVERLAY", -1, &tr, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
            SelectObject(dc, of);

            hLine(dc, 0, SIDEBAR_W, TAB_H-1, C_BORDER);
            vLine(dc, SIDEBAR_W, 0, WIN_H, C_BORDER);

            EndPaint(hw, &ps);
            return 0;
        }

        // Sidebar buttons are owner-drawn children of the main window
        case WM_DRAWITEM: {
            auto* di = (LPDRAWITEMSTRUCT)lp;
            int   id = (int)di->CtlID;
            if (id >= ID_TAB_BASE && id < ID_TAB_BASE + TAB_COUNT)
                paintTabBtn(di, id - ID_TAB_BASE);
            else
                paintButton(di);
            return TRUE;
        }

        case WM_COMMAND: {
            int id = LOWORD(wp);

            // Sidebar tab click
            if (id >= ID_TAB_BASE && id < ID_TAB_BASE + TAB_COUNT) {
                selectTab(id - ID_TAB_BASE);
                break;
            }

            // Route to the owning tab's handler
            // Each handler returns true if it consumed the message
            if (handleConfig (wp)) { log("[Config] Applied — process: %s  poll: %lums", s_config.targetProcess, s_config.pollMs); break; }
            if (handleCombat(wp)) { log("[Combat] Settings updated"); break; }
            if (handleESP   (wp)) { log("[ESP] Settings updated"); break; }
            if (handleMovement(wp)) { log("[Movement] Settings updated"); break; }
            if (handleMemory (wp)) { log("[Memory] Write not yet wired — fill game logic in main.cpp."); break; }
            if (handlePlayers(wp)) { log("[Players] Player list controls handled."); break; }
            handleHotkeys(wp);
            handleDebug  (wp);
            break;
        }

        case WM_MOUSEWHEEL: {
            if (s_activeTab >= 0 && s_activeTab < TAB_COUNT && s_panels[s_activeTab]) {
                SendMessage(s_panels[s_activeTab], WM_MOUSEWHEEL, wp, lp);
                return 0;
            }
            break;
        }

        case WM_CLOSE:
            ShowWindow(hw, SW_HIDE);
            return 0;
    }
    return DefWindowProcA(hw, msg, wp, lp);
}

// ============================================================
//  Public API
// ============================================================
bool init(HINSTANCE hInst) {
    s_hInst = hInst;
    initRes();

    INITCOMMONCONTROLSEX icex{ sizeof(icex), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    // Main window class
    const char* CLS = "OverlayGUI";
    WNDCLASSEXA wc{}; wc.cbSize = sizeof(wc);
    wc.lpfnWndProc   = GuiProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLS;
    wc.hbrBackground = s_brDark;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExA(&wc);

    s_hwnd = CreateWindowA(CLS, "Overlay Toolkit",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        180, 100, WIN_W, WIN_H,
        nullptr, nullptr, hInst, nullptr);
    if (!s_hwnd) return false;

    // Apply a thinner, modern window frame and title styling
    SetWindowTheme(s_hwnd, L"Explorer", nullptr);
    SetWindowLong(s_hwnd, GWL_STYLE, GetWindowLong(s_hwnd, GWL_STYLE) & ~WS_THICKFRAME);

    // Dark title bar (Windows 10 v1903+)
    BOOL dark = TRUE;
    DwmSetWindowAttribute(s_hwnd, 20, &dark, sizeof(dark));

    // Panel class (shared WndProc for all tab content areas)
    const char* PCLS = "OverlayPanel";
    WNDCLASSEXA pc{}; pc.cbSize = sizeof(pc);
    pc.lpfnWndProc   = PanelProc;
    pc.hInstance     = hInst;
    pc.lpszClassName = PCLS;
    pc.hbrBackground = s_brPanel;
    RegisterClassExA(&pc);

    // Sidebar tab buttons
    for (int i = 0; i < TAB_COUNT; ++i) {
        s_tabBtns[i] = CreateWindowA("BUTTON", TAB_NAMES[i],
            WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            0, TAB_H + i*TAB_H, SIDEBAR_W, TAB_H,
            s_hwnd, (HMENU)(intptr_t)(ID_TAB_BASE+i), hInst, nullptr);
        SendMessage(s_tabBtns[i], WM_SETFONT, (WPARAM)s_fUI, TRUE);
    }

    // Content panels
    int cx = SIDEBAR_W + 2;
    int cw = WIN_W - cx - 16;
    int ch = WIN_H - 42;
    for (int i = 0; i < TAB_COUNT; ++i) {
        int style = WS_CHILD | (i==0 ? WS_VISIBLE : 0) | WS_CLIPCHILDREN;
        s_panels[i] = CreateWindowA(PCLS, "",
            style,
            cx, 2, cw, ch,
            s_hwnd, nullptr, hInst, nullptr);
    }

    // Build each tab's controls
    buildConfig   (s_panels[0]);
    buildCombat   (s_panels[1]);
    buildESP      (s_panels[2]);
    buildMovement (s_panels[3]);
    buildMemory   (s_panels[4]);
    buildHotkeys  (s_panels[5]);
    buildDebug    (s_panels[6]);
    buildAbout    (s_panels[7]);
    buildPlayers  (s_panels[8]);

    return true;
}

void show()    { if (s_hwnd) ShowWindow(s_hwnd, SW_SHOW); }
void destroy() { freeRes(); if (s_hwnd) { DestroyWindow(s_hwnd); s_hwnd = nullptr; } }
HWND hwnd()    { return s_hwnd; }

void log(const char* fmt, ...) {
    char buf[512];
    va_list a; va_start(a, fmt);
    vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, a);
    va_end(a);

    if (s_panels[4]) {
        HWND ed = GetDlgItem(s_panels[4], ID_DBG_LOG);
        if (ed) {
            int len = GetWindowTextLengthA(ed);
            SendMessage (ed, EM_SETSEL,    (WPARAM)len, (LPARAM)len);
            SendMessageA(ed, EM_REPLACESEL, FALSE, (LPARAM)buf);
            SendMessageA(ed, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
            SendMessage (ed, EM_SCROLLCARET, 0, 0);
        }
    }
    printf("%s\n", buf);
}

void setStatus(const char* text) {
    if (s_panels[0]) {
        HWND st = GetDlgItem(s_panels[0], ID_CFG_STATUS);
        if (st) SetWindowTextA(st, text);
    }
}

Config       getConfig()  { return s_config;  }
VisualConfig getVisuals() { return s_visuals; }
void         setVisuals(const VisualConfig& cfg) { s_visuals = cfg; }
void         setAimbotEnabled(bool enabled) { s_visuals.aimbotEnabled = enabled; }

} // namespace Gui
