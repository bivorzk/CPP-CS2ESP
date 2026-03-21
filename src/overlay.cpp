#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "overlay.hpp"
#include "gui.hpp"      // for VisualConfig (applied on show)
#include <vector>

namespace Overlay {

static HWND s_hwnd    = nullptr;
static bool s_visible = false;
static std::vector<RECT> s_pawnRects;

static void drawStyledBox(HDC hdc, RECT r, COLORREF color) {
    // Hollow corner style (fancier than plain FrameRect)
    int thickness = 2;
    int corner = 16;

    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

    // top-left
    MoveToEx(hdc, r.left, r.top + corner, nullptr);
    LineTo(hdc, r.left, r.top);
    LineTo(hdc, r.left + corner, r.top);

    // top-right
    MoveToEx(hdc, r.right, r.top + corner, nullptr);
    LineTo(hdc, r.right, r.top);
    LineTo(hdc, r.right - corner, r.top);

    // bottom-right
    MoveToEx(hdc, r.right, r.bottom - corner, nullptr);
    LineTo(hdc, r.right, r.bottom);
    LineTo(hdc, r.right - corner, r.bottom);

    // bottom-left
    MoveToEx(hdc, r.left, r.bottom - corner, nullptr);
    LineTo(hdc, r.left, r.bottom);
    LineTo(hdc, r.left + corner, r.bottom);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            // Clear the whole layer first so we don't keep previous frame trails.
            HBRUSH clearBrush = CreateSolidBrush(RGB(0,0,0));
            FillRect(hdc, &rc, clearBrush);
            DeleteObject(clearBrush);

            // -- Hollow rectangle: paint only the 4 border strips.
            //    The interior is left unpainted (stays black → transparent
            //    via LWA_COLORKEY).
            Gui::VisualConfig vis = Gui::getVisuals();
            HBRUSH br = CreateSolidBrush(vis.color);
            int b = vis.borderPx;

            RECT top    = { 0,            0,            rc.right,     b            };
            RECT bottom = { 0,            rc.bottom - b, rc.right,    rc.bottom    };
            RECT left   = { 0,            b,            b,            rc.bottom - b };
            RECT right  = { rc.right - b, b,            rc.right,     rc.bottom - b };

            FillRect(hdc, &top,    br);
            FillRect(hdc, &bottom, br);
            FillRect(hdc, &left,   br);
            FillRect(hdc, &right,  br);

            DeleteObject(br);

            // Draw player pawn rectangles (if any)
            COLORREF boxColor = RGB(40,255,255);
            for (const RECT& r : s_pawnRects) {
                drawStyledBox(hdc, r, boxColor);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
        // No WM_DESTROY → PostQuitMessage here.
        // Quitting is controlled entirely from the main loop (END hotkey).
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

bool create(HINSTANCE hInst) {
    const char* CLS = "OverlayWindowClass";
    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLS;
    wc.hbrBackground = nullptr;
    RegisterClassExA(&wc);

    // Center on screen using current VisualConfig
    Gui::VisualConfig vis = Gui::getVisuals();
    int sw  = GetSystemMetrics(SM_CXSCREEN);
    int sh  = GetSystemMetrics(SM_CYSCREEN);
    int posX = (sw - vis.width)  / 2;
    int posY = (sh - vis.height) / 2;

    s_hwnd = CreateWindowExA(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLS, "",
        WS_POPUP,
        posX, posY, vis.width, vis.height,
        nullptr, nullptr, hInst, nullptr
    );
    if (!s_hwnd) return false;

    // Pure black (0x000000) becomes transparent — the hollow interior
    // is never painted, so it remains black → see-through.
    SetLayeredWindowAttributes(s_hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
    return true;
}

void show() {
    if (!s_visible && s_hwnd) {
        // Re-apply latest visual config (size, position, opacity) on each show
        Gui::VisualConfig vis = Gui::getVisuals();
        SetWindowPos(s_hwnd, HWND_TOPMOST,
                     vis.posX, vis.posY, vis.width, vis.height,
                     SWP_NOACTIVATE);
        SetLayeredWindowAttributes(s_hwnd, RGB(0,0,0), (BYTE)vis.opacity,
                                   LWA_COLORKEY | LWA_ALPHA);
        ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
        InvalidateRect(s_hwnd, nullptr, TRUE);
        s_visible = true;
    }
}

void hide() {
    if (s_visible && s_hwnd) {
        ShowWindow(s_hwnd, SW_HIDE);
        s_visible = false;
    }
}

void destroy() {
    if (s_hwnd) { DestroyWindow(s_hwnd); s_hwnd = nullptr; }
}

void repaint() {
    if (s_hwnd) InvalidateRect(s_hwnd, nullptr, TRUE);
}

HWND hwnd() { return s_hwnd; }

void setPawnRects(const std::vector<RECT>& rects) {
    s_pawnRects = rects;
    if (s_hwnd) InvalidateRect(s_hwnd, nullptr, TRUE);
}

void setOverlayBounds(int x, int y, int width, int height) {
    if (!s_hwnd) return;
    SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    InvalidateRect(s_hwnd, nullptr, TRUE);
}

void setOverlayPosition(int x, int y) {
    if (!s_hwnd) return;
    RECT rc;
    GetWindowRect(s_hwnd, &rc);
    SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, rc.right-rc.left, rc.bottom-rc.top,
                 SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
    InvalidateRect(s_hwnd, nullptr, TRUE);
}

} // namespace Overlay