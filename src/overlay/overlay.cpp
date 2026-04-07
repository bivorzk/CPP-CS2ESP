#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "overlay.hpp"
#include "gui.hpp"      // for VisualConfig (applied on show)
#include <cstdio>
#include <cmath>
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

static long long cross(const POINT& O, const POINT& A, const POINT& B) {
    return (long long)(A.x - O.x) * (B.y - O.y) - (long long)(A.y - O.y) * (B.x - O.x);
}

static std::vector<POINT> computeConvexHull(std::vector<POINT> points) {
    if (points.size() < 3) return points;
    std::sort(points.begin(), points.end(), [](const POINT& a, const POINT& b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    points.erase(std::unique(points.begin(), points.end(), [](const POINT& a, const POINT& b) {
        return a.x == b.x && a.y == b.y;
    }), points.end());
    if (points.size() < 3) return points;

    std::vector<POINT> lower, upper;
    for (const POINT& p : points) {
        while (lower.size() >= 2 && cross(lower[lower.size()-2], lower[lower.size()-1], p) <= 0)
            lower.pop_back();
        lower.push_back(p);
    }
    for (int i = (int)points.size() - 1; i >= 0; --i) {
        const POINT& p = points[i];
        while (upper.size() >= 2 && cross(upper[upper.size()-2], upper[upper.size()-1], p) <= 0)
            upper.pop_back();
        upper.push_back(p);
    }
    upper.pop_back();
    lower.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

static void drawFilledHull(HDC hdc, const std::vector<POINT>& points, COLORREF fillColor, COLORREF outlineColor, int outlineWidth = 3) {
    if (points.empty()) return;
    std::vector<POINT> hull = computeConvexHull(points);
    if (hull.size() < 3) return;

    HBRUSH fillBrush = CreateSolidBrush(fillColor);
    HPEN outlinePen = CreatePen(PS_SOLID, outlineWidth, outlineColor);
    HGDIOBJ oldBrush = SelectObject(hdc, fillBrush);
    HGDIOBJ oldPen = SelectObject(hdc, outlinePen);

    Polygon(hdc, hull.data(), (int)hull.size());

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(fillBrush);
    DeleteObject(outlinePen);
}

static void drawHealthBar(HDC hdc, const RECT& rect, int health, bool teamA) {
    int barW = 8;
    int barPadding = 10;
    int barLeft = rect.left - barPadding - barW;
    int barRight = barLeft + barW;
    int barTop = rect.top;
    int barBottom = rect.bottom;
    if (barLeft < 0) barLeft = rect.left + barPadding;
    if (barRight > rect.right + 100) barRight = rect.right + barPadding;

    float fraction = std::clamp(health / 100.0f, 0.0f, 1.0f);
    int fullH = barBottom - barTop;
    int fillH = (int)(fullH * fraction);
    int fillTop = barBottom - fillH;

    COLORREF bgColor = RGB(40, 40, 40);
    int red = (int)(255 * (1.0f - fraction));
    int green = (int)(255 * fraction);
    COLORREF fillColor = RGB(red, green, 0);

    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    RECT bgRect = {barLeft, barTop, barRight, barBottom};
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    HBRUSH fillBrush = CreateSolidBrush(fillColor);
    RECT fillRect = {barLeft + 1, fillTop + 1, barRight - 1, barBottom - 1};
    FillRect(hdc, &fillRect, fillBrush);
    DeleteObject(fillBrush);

    HPEN outlinePen = CreatePen(PS_SOLID, 1, RGB(220,220,220));
    HGDIOBJ oldPen = SelectObject(hdc, outlinePen);
    Rectangle(hdc, barLeft, barTop, barRight, barBottom);
    SelectObject(hdc, oldPen);
    DeleteObject(outlinePen);
}

static const std::pair<int,int> s_skeletonEdges[] = {
    {6, 5}, {5, 4}, {4, 0},
    {0, 22}, {22, 23}, {23, 24},
    {0, 25}, {25, 26}, {26, 27},
    {5, 8}, {8, 9}, {9, 11},
    {5, 13}, {13, 14}, {14, 16}
};

static const POINT* findBonePoint(const std::vector<std::pair<int, POINT>>& points, int boneId) {
    for (const auto& pair : points) {
        if (pair.first == boneId) return &pair.second;
    }
    return nullptr;
}

static bool isKeyJoint(int boneId) {
    switch (boneId) {
        case 0:  // pelvis/hips
        case 5:  // spine
        case 8:  // chest
        case 9:  // neck
        case 11: // head
        case 13: // left shoulder
        case 14: // left elbow
        case 16: // left wrist
        case 22: // right shoulder
        case 23: // right elbow
        case 24: // right wrist
            return true;
        default:
            return false;
    }
}

static void drawSkeleton(HDC hdc, const std::vector<std::pair<int, POINT>>& bonePoints, COLORREF lineColor, COLORREF jointColor, int thickness = 2) {
    LOGBRUSH lb{};
    lb.lbStyle = BS_SOLID;
    lb.lbColor = lineColor;
    HPEN bonePen = ExtCreatePen(PS_GEOMETRIC | PS_JOIN_ROUND | PS_ENDCAP_ROUND, thickness, &lb, 0, nullptr);
    HBRUSH jointBrush = CreateSolidBrush(jointColor);
    int prevMode = SetGraphicsMode(hdc, GM_ADVANCED);
    HGDIOBJ oldPen = SelectObject(hdc, bonePen);
    HGDIOBJ oldBrush = SelectObject(hdc, jointBrush);

    for (const auto& edge : s_skeletonEdges) {
        const POINT* a = findBonePoint(bonePoints, edge.first);
        const POINT* b = findBonePoint(bonePoints, edge.second);
        if (a && b) {
            MoveToEx(hdc, a->x, a->y, nullptr);
            LineTo(hdc, b->x, b->y);
        }
    }

    for (const auto& item : bonePoints) {
        if (!isKeyJoint(item.first)) continue;
        const POINT& pt = item.second;
        int r = 1;
        Ellipse(hdc, pt.x - r, pt.y - r, pt.x + r, pt.y + r);
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(bonePen);
    DeleteObject(jointBrush);
    SetGraphicsMode(hdc, prevMode);
}

static COLORREF blendColor(COLORREF a, COLORREF b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    int ar = GetRValue(a), ag = GetGValue(a), ab = GetBValue(a);
    int br = GetRValue(b), bg = GetGValue(b), bb = GetBValue(b);
    return RGB((int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t), (int)(ab + (bb - ab) * t));
}

static void drawPawnEsp(HDC hdc, const Overlay::PawnRenderInfo& p) {
    std::vector<POINT> points;
    points.reserve(p.bonePoints.size());
    for (const auto& pair : p.bonePoints) points.push_back(pair.second);

    int espMode = Gui::getVisuals().espMode;
    int strength = std::max(1, std::min(5, Gui::getVisuals().espStrength));
    int baseWidth = 1;
    float pulse = 0.5f + 0.5f * std::sin(GetTickCount64() / 250.0);

    if (espMode == 0) {
        if (!points.empty()) {
            drawFilledHull(hdc, points, RGB(30, 140, 220), RGB(100, 210, 255), 2 + strength);
            drawSkeleton(hdc, p.bonePoints, blendColor(RGB(255,255,255), RGB(140,220,255), pulse), RGB(210, 240, 255), baseWidth);
        } else if (p.drawBox) {
            drawStyledBox(hdc, p.rect, RGB(30, 140, 220));
        }
    } else if (espMode == 1) {
        if (!points.empty()) {
            COLORREF base = p.teamA ? RGB(80, 170, 255) : RGB(255, 220, 80);
            COLORREF glow = p.teamA ? RGB(120, 205, 255) : RGB(255, 245, 160);
            drawSkeleton(hdc, p.bonePoints, blendColor(base, glow, pulse), RGB(255, 255, 200), 2 + strength);
        } else if (p.drawBox) {
            drawStyledBox(hdc, p.rect, RGB(255, 220, 80));
        }
    } else if (espMode == 2) {
        if (!points.empty()) {
            COLORREF fill = RGB(200, 20, 20);
            COLORREF outline = blendColor(RGB(255, 70, 70), RGB(255, 180, 180), pulse);
            drawFilledHull(hdc, points, fill, outline, 2 + strength);
            // Flat chams mode should remain clean and non-skeletal.
        } else if (p.drawBox) {
            drawStyledBox(hdc, p.rect, RGB(200, 20, 20));
        }
    }
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

            // Draw player pawn rectangles, ESP shapes, and bomb markers
            for (const Overlay::PawnRenderInfo& p : s_pawnRects) {
                if (!p.isBomb && p.bonePoints.empty() && !p.drawBox) continue;

                if (p.isBomb) {
                    drawStyledBox(memDC, p.rect, RGB(255, 120, 0));
                } else {
                    drawPawnEsp(memDC, p);
                }

                // name/label above the box or ESP shape
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

                if (!p.isBomb) {
                    drawHealthBar(memDC, p.rect, p.health, p.teamA);
                }
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
        InvalidateRect(s_hwnd, nullptr, FALSE);
        PostMessageA(s_hwnd, WM_PAINT, 0, 0);
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