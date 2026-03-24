#include "gui_tabs.hpp"
#include "gui_draw.hpp"
#include <cstdio>
#include <algorithm>
#include <cmath>

// ============================================================
//  gui_tab_players.cpp
//  5v5 player display — names + colour-coded health bars.
//
//  Feed data from main.cpp:
//    Gui::updatePlayer(team, slot, name, hp, hpMax, alive);
//
//  team  : 0 = Team A (left),  1 = Team B (right)
//  slot  : 0–4
// ============================================================

namespace Gui {

// ============================================================
//  Player data store
// ============================================================
struct PlayerData {
    char name[32] = "Player";
    int  hp       = 100;
    int  hpMax    = 100;
    bool alive    = true;
};

static PlayerData s_team[2][5];   // s_team[team][slot]
static HWND       s_playersPanel  = nullptr;
static HWND       s_bombTimerLabel = nullptr;

static constexpr int ID_BOMB_TIMER = 630;

// ============================================================
//  Control ID scheme
//  Name edits:  ID_PLR_BASE + (team*5 + slot)     →  600–609
//  Health bars: ID_PLR_BASE + 10 + (team*5 + slot) → 610–619
//  Team headers: 620, 621
// ============================================================
static constexpr int ID_PLR_BASE = 600;

inline int nameId(int team, int slot) { return ID_PLR_BASE      + team*5 + slot; }
inline int barId (int team, int slot) { return ID_PLR_BASE + 10 + team*5 + slot; }

// ============================================================
//  Public update — call from pollTimer in main.cpp
// ============================================================
void updatePlayer(int team, int slot,
                  const char* name, int hp, int hpMax, bool alive)
{
    if (team < 0 || team > 1 || slot < 0 || slot > 4) return;
    auto& p = s_team[team][slot];

    // Keep existing name if update is empty (used by overlay team-A suppression path)
    if (name && name[0] != '\0') {
        strncpy_s(p.name, name, 31);
    }

    // Keep existing health values if not valid in current update
    if (hp > 0) p.hp = hp;
    if (hpMax > 0) p.hpMax = hpMax;

    p.alive = alive;

    // Refresh name box
    if (s_playersPanel) {
        HWND ne = GetDlgItem(s_playersPanel, nameId(team, slot));
        if (ne) SetWindowTextA(ne, p.name);
        // Force health bar repaint
        HWND be = GetDlgItem(s_playersPanel, barId(team, slot));
        if (be) InvalidateRect(be, nullptr, FALSE);
    }
}

void updateBombTimer(float secondsRemaining, bool active) {
    if (!s_playersPanel || !s_bombTimerLabel) return;
    char text[64];
    if (!active || secondsRemaining <= 0.0f) {
        sprintf_s(text, "Bomb Timer: N/A");
    } else {
        int sec = (int)ceil(secondsRemaining);
        int mins = sec / 60;
        int secs = sec % 60;
        sprintf_s(text, "Bomb Timer: %d:%02d  (%.1fs)", mins, secs, secondsRemaining);
    }
    SetWindowTextA(s_bombTimerLabel, text);
}

// ============================================================
//  Health bar painting
// ============================================================
static void paintHealthBar(HDC dc, RECT rc, const PlayerData& p) {
    fillRC(dc, rc, C_BG_CTRL);
    outlineRC(dc, rc, C_BORDER);

    if (!p.alive) {
        if (p.name[0] == '\0') {
            // no player mapped to this slot; keep bar empty
            return;
        }
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_TEXT_SEC);
        HFONT of = (HFONT)SelectObject(dc, s_fMono);
        DrawTextA(dc, "DEAD", -1, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(dc, of);
        return;
    }

    float frac = std::clamp((float)p.hp / (float)p.hpMax, 0.f, 1.f);

    COLORREF col = frac > 0.60f ? RGB( 50, 200,  80)   // green
                 : frac > 0.30f ? RGB(220, 180,  30)   // yellow
                                : RGB(210,  45,  45);  // red

    int fillW = (int)((rc.right - rc.left - 2) * frac);
    if (fillW > 0) {
        RECT fill = { rc.left+1, rc.top+1, rc.left+1+fillW, rc.bottom-1 };
        fillRC(dc, fill, col);
        // Highlight stripe along top edge
        RECT shine = { fill.left, fill.top, fill.right, fill.top+1 };
        fillRC(dc, shine, RGB(
            (std::min)(255, (int)GetRValue(col)+70),
            (std::min)(255, (int)GetGValue(col)+70),
            (std::min)(255, (int)GetBValue(col)+70)
        ));
    }

    // HP label
    char txt[24]; sprintf_s(txt, "%d / %d", p.hp, p.hpMax);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, C_TEXT_PRI);
    HFONT of = (HFONT)SelectObject(dc, s_fMono);
    DrawTextA(dc, txt, -1, &rc, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(dc, of);
}

// ============================================================
//  Panel WndProc — intercepts WM_DRAWITEM for health bars,
//                  delegates everything else to PanelProc
// ============================================================
static LRESULT CALLBACK PlayersPanelProc(HWND hw, UINT msg,
                                          WPARAM wp, LPARAM lp)
{
    if (msg == WM_DRAWITEM) {
        auto* di = (LPDRAWITEMSTRUCT)lp;
        int   id = (int)di->CtlID;
        if (id >= ID_PLR_BASE+10 && id < ID_PLR_BASE+20) {
            int idx  = id - (ID_PLR_BASE+10);
            paintHealthBar(di->hDC, di->rcItem,
                           s_team[idx / 5][idx % 5]);
            return TRUE;
        }
    }
    return PanelProc(hw, msg, wp, lp);
}

// ============================================================
//  Build
// ============================================================
void buildPlayers(HWND p) {
    s_playersPanel = p;
    SetWindowLongPtrA(p, GWLP_WNDPROC, (LONG_PTR)PlayersPanelProc);

    RECT rc; GetClientRect(p, &rc);
    int panelW = rc.right - rc.left;
    int colW   = (panelW - 40) / 2;
    int col1X  = 14;
    int col2X  = col1X + colW + 12;
    int nameW  = 96;
    int barW   = colW - nameW - 8;
    int barH   = 20;
    int startY = 46;
    int rowH   = 30;

    // Team headers
    auto makeHeader = [&](const char* t, int x, int id) {
        HWND hw = CreateWindowA("STATIC", t,
            WS_CHILD|WS_VISIBLE|SS_LEFT,
            x, 14, colW, 18, p, (HMENU)(intptr_t)id, s_hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)s_fBold, TRUE);
    };
    makeHeader("TEAM  A", col1X,  ID_PLR_BASE+20);
    makeHeader("TEAM  B", col2X,  ID_PLR_BASE+21);

    // Divider line under headers (drawn via a 1px-tall static)
    auto makeDivider = [&](int x) {
        HWND hw = CreateWindowA("STATIC", "",
            WS_CHILD|WS_VISIBLE|SS_ETCHEDHORZ,
            x, 36, colW, 2, p, nullptr, s_hInst, nullptr);
        (void)hw;
    };
    makeDivider(col1X);
    makeDivider(col2X);

    // Player rows
    for (int i = 0; i < 5; ++i) {
        int y = startY + i * rowH;

        for (int t = 0; t < 2; ++t) {
            int x = (t == 0) ? col1X : col2X;

            // Name box (read-only, populated by updatePlayer)
            HWND ne = CreateWindowA("EDIT",
                s_team[t][i].name,
                WS_CHILD|WS_VISIBLE|ES_READONLY|ES_LEFT,
                x, y, nameW, barH,
                p, (HMENU)(intptr_t)nameId(t,i), s_hInst, nullptr);
            SendMessage(ne, WM_SETFONT, (WPARAM)s_fMono, TRUE);

            // Health bar (owner-draw button — no text, painted in WM_DRAWITEM)
            CreateWindowA("BUTTON", "",
                WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
                x + nameW + 4, y, barW, barH,
                p, (HMENU)(intptr_t)barId(t,i), s_hInst, nullptr);
        }
    }

    // Bomb timer label (shared component in Players tab)
    int timerY = startY + 5 * rowH + 6;
    s_bombTimerLabel = CreateWindowA("STATIC", "Bomb Timer: N/A",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        col1X, timerY, panelW - 20, 20,
        p, (HMENU)(intptr_t)ID_BOMB_TIMER, s_hInst, nullptr);
    SendMessage(s_bombTimerLabel, WM_SETFONT, (WPARAM)s_fMono, TRUE);
}

bool handlePlayers(WPARAM) { return false; }

void clearPlayers() {
    HWND lb = GetDlgItem(s_playersPanel, ID_PLAYERS_LIST);
    if (lb) {
        SendMessageA(lb, LB_RESETCONTENT, 0, 0);
    }
    for (int t = 0; t < 2; ++t) {
        for (int s = 0; s < 5; ++s) {
            s_team[t][s].hp = 100;
            s_team[t][s].hpMax = 100;
            s_team[t][s].alive = false;
            s_team[t][s].name[0] = '\0';
            HWND ne = GetDlgItem(s_playersPanel, nameId(t, s));
            if (ne) SetWindowTextA(ne, "");
            HWND be = GetDlgItem(s_playersPanel, barId(t, s));
            if (be) InvalidateRect(be, nullptr, FALSE);
        }
    }
    if (s_bombTimerLabel) {
        SetWindowTextA(s_bombTimerLabel, "Bomb Timer: N/A");
    }
}

} // namespace Gui
