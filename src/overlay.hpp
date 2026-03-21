#pragma once
#include <windows.h>
#include <vector>

// ============================================================
//  overlay.hpp — Transparent always-on-top hollow overlay
//
//  Lifecycle:
//    Overlay::create(hInst)  — call after Gui::init()
//    Overlay::show()         — re-applies latest VisualConfig
//    Overlay::hide()
//    Overlay::repaint()      — force WM_PAINT (e.g. after config change)
//    Overlay::destroy()
// ============================================================

namespace Overlay {
    bool create(HINSTANCE hInst);
    void show();
    void hide();
    void repaint();
    void destroy();
    HWND hwnd();

    // World-space markers to draw (e.g., players).
    void setPawnRects(const std::vector<RECT>& rects);

    // Move/resize the overlay window to match target screen bounds.
    void setOverlayBounds(int x, int y, int width, int height);

    // Move the overlay window (hollow rectangle) to follow a target screen point.
    void setOverlayPosition(int x, int y);
}