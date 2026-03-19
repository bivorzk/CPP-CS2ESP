#include "overlay.hpp"

// ============================================================
//  Overlay config — tweak freely
// ============================================================
static constexpr int   RECT_W        = 300;   // hollow rect width
static constexpr int   RECT_H        = 200;   // hollow rect height
static constexpr int   BORDER_PX     = 3;     // outline thickness
static constexpr COLORREF RECT_COLOR = RGB(255, 255, 255); // white
// ============================================================

namespace Overlay {

static HWND s_hwnd   = nullptr;
static bool s_visible = false;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg,
                                 WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);

            // Hollow rectangle — fill only the border region.
            // Done by drawing 4 filled rects along each edge.
            HBRUSH brush = CreateSolidBrush(RECT_COLOR);

            // Top bar
            RECT top    = { 0, 0, rc.right, BORDER_PX };
            // Bottom bar
            RECT bottom = { 0, rc.bottom - BORDER_PX, rc.right, rc.bottom };
            // Left bar
            RECT left   = { 0, BORDER_PX, BORDER_PX, rc.bottom - BORDER_PX };
            // Right bar
            RECT right  = { rc.right - BORDER_PX, BORDER_PX,
                             rc.right, rc.bottom - BORDER_PX };

            FillRect(hdc, &top,    brush);
            FillRect(hdc, &bottom, brush);
            FillRect(hdc, &left,   brush);
            FillRect(hdc, &right,  brush);

            DeleteObject(brush);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

bool create(HINSTANCE hInst) {
    const char* CLASS_NAME = "OverlayWindowClass";

    WNDCLASSEX wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLASS_NAME;
    // Null background brush so the OS doesn't paint over our WM_PAINT
    wc.hbrBackground = nullptr;
    RegisterClassEx(&wc);

    // Center on screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX    = (screenW - RECT_W) / 2;
    int posY    = (screenH - RECT_H) / 2;

    // WS_EX_LAYERED    — required for alpha / transparency
    // WS_EX_TRANSPARENT — mouse clicks fall through to windows below
    // WS_EX_TOPMOST    — always above every other window
    // WS_EX_TOOLWINDOW — hidden from Alt+Tab
    s_hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME, "",
        WS_POPUP,
        posX, posY, RECT_W, RECT_H,
        nullptr, nullptr, hInst, nullptr
    );
    if (!s_hwnd) return false;

    // LWA_COLORKEY makes any pixel of color 0x000000 (pure black)
    // fully transparent — the hollow centre of our rect becomes
    // see-through without needing a layered alpha per-pixel.
    SetLayeredWindowAttributes(s_hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
    return true;
}

void show() {
    if (!s_visible && s_hwnd) {
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
    if (s_hwnd) {
        DestroyWindow(s_hwnd);
        s_hwnd = nullptr;
    }
}

HWND hwnd() { return s_hwnd; }

} // namespace Overlay
