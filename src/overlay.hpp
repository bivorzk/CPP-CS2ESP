#pragma once
#include <windows.h>

// ============================================================
//  overlay.hpp — Transparent always-on-top hollow overlay
//
//  Call once:   Overlay::create(hInstance)
//  Show/hide:   Overlay::show() / Overlay::hide()
//  Cleanup:     Overlay::destroy()
// ============================================================

namespace Overlay {

    bool    create(HINSTANCE hInst);
    void    show();
    void    hide();
    void    destroy();
    HWND    hwnd();

} // namespace Overlay
