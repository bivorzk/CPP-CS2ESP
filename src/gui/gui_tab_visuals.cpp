#include "gui_tabs.hpp"
#include "gui_draw.hpp"
#include <cstdio>
#include <cstdlib>

namespace Gui {

void buildCombat(HWND p) {
    int x=18, y=18;

    mkHeader(p, "Combat", x, y); y += 26;

    HWND cbAutoAim = CreateWindowA("BUTTON", "Auto-aim mode",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_VIS_AUTO_AIM, s_hInst, nullptr);
    SendMessage(cbAutoAim, BM_SETCHECK, s_visuals.autoAim ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbAltFire = CreateWindowA("BUTTON", "ALT shoots when held",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_VIS_ALT_FIRE, s_hInst, nullptr);
    SendMessage(cbAltFire, BM_SETCHECK, s_visuals.altAutoFire ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbStrict = CreateWindowA("BUTTON", "Strict visibility (damage cone)",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_VIS_STRICT_VIS, s_hInst, nullptr);
    SendMessage(cbStrict, BM_SETCHECK, s_visuals.strictVisibility ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    mkLabel(p, "Aim target body part", x, y, 120, 16);
    HWND cbPart = CreateWindowA("COMBOBOX", "",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        x+132, y-2, 140, 120,
        p, (HMENU)(intptr_t)ID_VIS_AIM_PART, s_hInst, nullptr);
    SendMessage(cbPart, CB_ADDSTRING, 0, (LPARAM)"Head");
    SendMessage(cbPart, CB_ADDSTRING, 0, (LPARAM)"Body");
    SendMessage(cbPart, CB_ADDSTRING, 0, (LPARAM)"Arms");
    SendMessage(cbPart, CB_ADDSTRING, 0, (LPARAM)"Legs");
    SendMessage(cbPart, CB_SETCURSEL, s_visuals.aimPart, 0);
    y += 34;

    mkLabel(p, "Target switch delay", x, y, 130, 16);
    char buf[16];
    sprintf_s(buf, "%d", s_visuals.visCooldownFrames);
    mkEdit(p, buf, x+140, y-2, 50, 22, ID_VIS_COOLDOWN);
    y += 36;

    mkButton(p, "Apply Combat", x, y, 120, 28, ID_VIS_APPLY);
}

void buildMovement(HWND p) {
    int x=18, y=18;

    mkHeader(p, "Movement", x, y); y += 26;

    HWND cbBhop = CreateWindowA("BUTTON", "Auto bhop while SPACE held",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_VIS_AUTO_BHOP, s_hInst, nullptr);
    SendMessage(cbBhop, BM_SETCHECK, s_visuals.autoBhop ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbTeam = CreateWindowA("BUTTON", "Show team boxes",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_VIS_SHOW_TEAM_A, s_hInst, nullptr);
    SendMessage(cbTeam, BM_SETCHECK, s_visuals.showTeamABoxes ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    mkHeader(p, "Interface", x, y); y += 24;
    mkLabel(p, "Opacity", x, y, 58, 16); y += 18;
    mkSlider(p, x, y, 260, 22, ID_VIS_OPACITY, 0, 255, s_visuals.opacity);
    mkValLbl(p, s_visuals.opacity, x+270, y, ID_VIS_OPACITY_LBL);
    y += 34;
    mkButton(p, "Apply Movement", x, y, 130, 28, ID_VIS_APPLY_MOVEMENT);
}

bool handleCombat(WPARAM wp) {
    switch (LOWORD(wp)) {
        case ID_VIS_APPLY: {
            HWND p = s_panels[1];
            s_visuals.autoAim = (SendMessage(GetDlgItem(p, ID_VIS_AUTO_AIM), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.altAutoFire = (SendMessage(GetDlgItem(p, ID_VIS_ALT_FIRE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.strictVisibility = (SendMessage(GetDlgItem(p, ID_VIS_STRICT_VIS), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.aimPart = (int)SendMessage(GetDlgItem(p, ID_VIS_AIM_PART), CB_GETCURSEL, 0, 0);
            char buf[16];
            GetWindowTextA(GetDlgItem(p, ID_VIS_COOLDOWN), buf, 16);
            s_visuals.visCooldownFrames = atoi(buf);
            if (s_visuals.visCooldownFrames < 0) s_visuals.visCooldownFrames = 0;
            return true;
        }
    }
    return false;
}

bool handleMovement(WPARAM wp) {
    switch (LOWORD(wp)) {
        case ID_VIS_APPLY_MOVEMENT: {
            HWND p = s_panels[2];
            s_visuals.autoBhop = (SendMessage(GetDlgItem(p, ID_VIS_AUTO_BHOP), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.showTeamABoxes = (SendMessage(GetDlgItem(p, ID_VIS_SHOW_TEAM_A), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.opacity = (int)SendMessage(GetDlgItem(p, ID_VIS_OPACITY), TBM_GETPOS, 0, 0);
            return true;
        }
    }
    return false;
}

} // namespace Gui
