#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "overlay.hpp"
#include "gui.hpp"      // for VisualConfig (applied on show)
#include <cstdio>
#include <vector>
#include <algorithm>

namespace Overlay {

static HWND s_hwnd    = nullptr;
static bool s_visible = false;
static std::vector<Overlay::PawnRenderInfo> s_pawnRects;

static void drawStyledBox(HDC hdc, RECT r, COLORREF color) {
    // Larger corner style with moderately thicker outline for better visibility
    int thickness = 3;
    int width = (int)std::max<long>(0, (long)(r.right - r.left));
    int height = (int)std::max<long>(0, (long)(r.bottom - r.top));
    int len = std::min(16, std::min(width / 4, height / 4));

    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    // Top-left
    MoveToEx(hdc, r.left, r.top + len, nullptr);
    LineTo(hdc, r.left, r.top);
    LineTo(hdc, r.left + len, r.top);

    // Top-right
    MoveToEx(hdc, r.right, r.top + len, nullptr);
    LineTo(hdc, r.right, r.top);
    LineTo(hdc, r.right - len, r.top);

    // Bottom-right
    MoveToEx(hdc, r.right, r.bottom - len, nullptr);
    LineTo(hdc, r.right, r.bottom);
    LineTo(hdc, r.right - len, r.bottom);

    // Bottom-left
    MoveToEx(hdc, r.left, r.bottom - len, nullptr);
    LineTo(hdc, r.left, r.bottom);
    LineTo(hdc, r.left + len, r.bottom);

    // Optional center cross for quick aim overscan
    int cx = (r.left + r.right) / 2;
    int cy = (r.top + r.bottom) / 2;
    int cs = std::min(4, len);
    MoveToEx(hdc, cx - cs, cy, nullptr);
    LineTo(hdc, cx + cs, cy);
    MoveToEx(hdc, cx, cy - cs, nullptr);
    LineTo(hdc, cx, cy + cs);

    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ERASEBKGND: {
            // Prevent flicker and white “not painted yet” content on composited fullscreen.
            return 1;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right, h = rc.bottom;

            static HBRUSH s_outlineBrush = CreateSolidBrush(RGB(50,50,50));
            static HFONT  s_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

            // Double-buffer: draw everything off-screen first, then blit in one shot.
            // This eliminates the white flash caused by the window being briefly unpainted.
            HDC memDC  = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

            // Clear to color key (magenta = transparent)
            HBRUSH clearBrush = CreateSolidBrush(RGB(255,0,255));
            FillRect(memDC, &rc, clearBrush);
            DeleteObject(clearBrush);

            // Draw player pawn rectangles (and bomb markers)
            for (const Overlay::PawnRenderInfo& p : s_pawnRects) {
                if (!p.drawBox && !p.isBomb) continue;

                COLORREF boxColor;
                if (p.isBomb) {
                    boxColor = RGB(255, 120, 0);
                } else {
                    boxColor = p.teamA ? RGB(100,255,100) : RGB(40,255,255);
                }
                if (p.drawBox) drawStyledBox(memDC, p.rect, boxColor);

                // name/label above the box
                SetBkMode(memDC, TRANSPARENT);
                SetTextColor(memDC, RGB(255,255,255));
                SelectObject(memDC, s_font);
                int textX = p.rect.left;
                int textY = p.rect.top - 16;

                char label[64];
                if (p.isBomb) {
                    float remaining = p.blowTime;
                    if (remaining < 0.0f) remaining = 0.0f;
                    sprintf_s(label, "BOMB %.1f", remaining);
                } else {
                    strncpy_s(label, p.name, _TRUNCATE);
                }
                TextOutA(memDC, textX, textY, label, (int)strlen(label));

                // vertical health bar
                int barW = 6;
                int barPadding = 4;
                int barTop    = p.rect.top + 1;
                int barBottom = p.rect.bottom - 1;
                int barLeft   = p.rect.right + barPadding;
                int barRight  = barLeft + barW;

                float frac   = std::clamp(p.health / 100.0f, 0.0f, 1.0f);
                int fullH    = barBottom - barTop;
                int filledH  = (int)(fullH * frac);
                int fillTop  = barBottom - filledH;

                RECT baseBar = { barLeft, barTop, barRight, barBottom };
                FrameRect(memDC, &baseBar, s_outlineBrush);

                COLORREF hColor = RGB(
                    (BYTE)(255 * (1.0f - frac)),
                    (BYTE)(255 * frac),
                    0);
                RECT fillBar = { barLeft+1, fillTop, barRight-1, barBottom-1 };
                HBRUSH hBrush = CreateSolidBrush(hColor);
                FillRect(memDC, &fillBar, hBrush);
                DeleteObject(hBrush);
            }

            // Blit the completed frame to the screen in one atomic operation
            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

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
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        CLS, "",
        WS_POPUP,
        posX, posY, vis.width, vis.height,
        nullptr, nullptr, hInst, nullptr
    );
    if (!s_hwnd) return false;

    // The overlay content is drawn to magenta (0xFF00FF) when cleared, then treated as transparent.
    SetLayeredWindowAttributes(s_hwnd, RGB(255,0,255), 0, LWA_COLORKEY);
    return true;
}

void show() {
    if (!s_visible && s_hwnd) {
        // Re-apply latest visual config (size, position, opacity) on each show
        Gui::VisualConfig vis = Gui::getVisuals();
        SetWindowPos(s_hwnd, HWND_TOPMOST,
                     vis.posX, vis.posY, vis.width, vis.height,
                     SWP_NOACTIVATE);
        SetLayeredWindowAttributes(s_hwnd, RGB(255,0,255), (BYTE)vis.opacity,
                                   LWA_COLORKEY | LWA_ALPHA);
        ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
        InvalidateRect(s_hwnd, nullptr, FALSE);
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
    if (s_hwnd) InvalidateRect(s_hwnd, nullptr, FALSE);
}

HWND hwnd() { return s_hwnd; }

void setPawnRects(const std::vector<PawnRenderInfo>& pawns) {
    s_pawnRects = pawns;
    if (s_hwnd) {
        // Invalidate async and let the message queue handle paint. This is more
        // efficient than blocking UpdateWindow() every frame.
        InvalidateRect(s_hwnd, nullptr, FALSE);
    }
}

void setOverlayBounds(int x, int y, int width, int height) {
    if (!s_hwnd) return;
    SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_NOZORDER);
    InvalidateRect(s_hwnd, nullptr, FALSE);
}

void setOverlayPosition(int x, int y) {
    if (!s_hwnd) return;
    RECT rc;
    GetWindowRect(s_hwnd, &rc);
    SetWindowPos(s_hwnd, HWND_TOPMOST, x, y, rc.right-rc.left, rc.bottom-rc.top,
                 SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
    InvalidateRect(s_hwnd, nullptr, FALSE);
}

} // namespace Overlay