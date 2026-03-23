#include "gui_tabs.hpp"
#include "gui_draw.hpp"
#include <cstdio>

namespace Gui {

void buildVisuals(HWND p) {
    int x=18, y=18;
    char buf[16];

    mkHeader(p, "DIMENSIONS", x, y); y+=24;
    mkLabel(p, "Width",      x,      y, 58, 16);
    mkLabel(p, "Height",     x+74,   y, 58, 16);
    mkLabel(p, "Border px",  x+150,  y, 64, 16);
    y+=17;
    sprintf_s(buf,"%d",s_visuals.width);    mkEdit(p,buf, x,     y,66,24,ID_VIS_WIDTH);
    sprintf_s(buf,"%d",s_visuals.height);   mkEdit(p,buf, x+74,  y,66,24,ID_VIS_HEIGHT);
    sprintf_s(buf,"%d",s_visuals.borderPx); mkEdit(p,buf, x+150, y,54,24,ID_VIS_BORDER);
    y+=36;

    mkHeader(p, "COLOUR & OPACITY", x, y); y+=24;
    mkLabel(p, "Opacity", x, y, 54, 16); y+=16;
    mkSlider(p, x,    y, 250, 20, ID_VIS_OPACITY, 0, 255, s_visuals.opacity);
    mkValLbl(p, s_visuals.opacity, x+256, y, ID_VIS_OPACITY_LBL);
    y+=26;

    mkLabel(p,"R",x,y,14,16); y+=15;
    mkSlider(p,x,y,250,16,ID_VIS_R,0,255,GetRValue(s_visuals.color));
    mkValLbl(p,GetRValue(s_visuals.color),x+256,y,ID_VIS_R_LBL); y+=22;

    mkLabel(p,"G",x,y,14,16); y+=15;
    mkSlider(p,x,y,250,16,ID_VIS_G,0,255,GetGValue(s_visuals.color));
    mkValLbl(p,GetGValue(s_visuals.color),x+256,y,ID_VIS_G_LBL); y+=22;

    mkLabel(p,"B",x,y,14,16); y+=15;
    mkSlider(p,x,y,250,16,ID_VIS_B,0,255,GetBValue(s_visuals.color));
    mkValLbl(p,GetBValue(s_visuals.color),x+256,y,ID_VIS_B_LBL); y+=28;

    HWND cb = CreateWindowA("BUTTON", "Team A boxes",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 150, 20,
        p, (HMENU)(intptr_t)ID_VIS_SHOW_TEAM_A, s_hInst, nullptr);
    SendMessage(cb, BM_SETCHECK, s_visuals.showTeamABoxes ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 28;

    HWND cb2 = CreateWindowA("BUTTON", "Auto-aim & shoot (no key)",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 20,
        p, (HMENU)(intptr_t)ID_VIS_AUTO_AIM, s_hInst, nullptr);
    SendMessage(cb2, BM_SETCHECK, s_visuals.autoAim ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 26;

    HWND cb3 = CreateWindowA("BUTTON", "Strict visibility (distance + cone)",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 20,
        p, (HMENU)(intptr_t)ID_VIS_STRICT_VIS, s_hInst, nullptr);
    SendMessage(cb3, BM_SETCHECK, s_visuals.strictVisibility ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 26;

    mkLabel(p, "Aim target part", x, y, 100, 16);
    HWND cb4 = CreateWindowA("COMBOBOX", "",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        x+110, y-2, 130, 120,
        p, (HMENU)(intptr_t)ID_VIS_AIM_PART, s_hInst, nullptr);
    SendMessage(cb4, CB_ADDSTRING, 0, (LPARAM)"Head");
    SendMessage(cb4, CB_ADDSTRING, 0, (LPARAM)"Body");
    SendMessage(cb4, CB_ADDSTRING, 0, (LPARAM)"Arms");
    SendMessage(cb4, CB_ADDSTRING, 0, (LPARAM)"Legs");
    SendMessage(cb4, CB_SETCURSEL, s_visuals.aimPart, 0);
    y += 34;

    mkLabel(p, "Target hold frames", x, y, 120, 16);
    char buf2[16];
    sprintf_s(buf2, "%d", s_visuals.visCooldownFrames);
    mkEdit(p, buf2, x+130, y-2, 48, 22, ID_VIS_COOLDOWN);
    y += 30;

    mkButton(p, "Apply",            x,     y, 90, 26, ID_VIS_APPLY);
    mkButton(p, "Center on Screen", x+98,  y,130, 26, ID_VIS_CENTER);
}

bool handleVisuals(WPARAM wp) {
    switch (LOWORD(wp)) {
        case ID_VIS_APPLY: {
            HWND p = s_panels[1];
            char buf[16];
            GetWindowTextA(GetDlgItem(p,ID_VIS_WIDTH),  buf,16); s_visuals.width    = atoi(buf);
            GetWindowTextA(GetDlgItem(p,ID_VIS_HEIGHT), buf,16); s_visuals.height   = atoi(buf);
            GetWindowTextA(GetDlgItem(p,ID_VIS_BORDER), buf,16); s_visuals.borderPx = atoi(buf);
            s_visuals.opacity = (int)SendMessage(GetDlgItem(p,ID_VIS_OPACITY), TBM_GETPOS, 0,0);
            int r = (int)SendMessage(GetDlgItem(p,ID_VIS_R), TBM_GETPOS, 0,0);
            int g = (int)SendMessage(GetDlgItem(p,ID_VIS_G), TBM_GETPOS, 0,0);
            int b = (int)SendMessage(GetDlgItem(p,ID_VIS_B), TBM_GETPOS, 0,0);
            s_visuals.color = RGB(r,g,b);
            s_visuals.showTeamABoxes = (SendMessage(GetDlgItem(p, ID_VIS_SHOW_TEAM_A), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.autoAim        = (SendMessage(GetDlgItem(p, ID_VIS_AUTO_AIM), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.strictVisibility = (SendMessage(GetDlgItem(p, ID_VIS_STRICT_VIS), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.aimPart = (int)SendMessage(GetDlgItem(p, ID_VIS_AIM_PART), CB_GETCURSEL, 0, 0);
            GetWindowTextA(GetDlgItem(p, ID_VIS_COOLDOWN), buf, 16); s_visuals.visCooldownFrames = atoi(buf);
            if (s_visuals.visCooldownFrames < 0) s_visuals.visCooldownFrames = 0;
            return true;
        }
        case ID_VIS_CENTER:
            s_visuals.posX = (GetSystemMetrics(SM_CXSCREEN)-s_visuals.width)/2;
            s_visuals.posY = (GetSystemMetrics(SM_CYSCREEN)-s_visuals.height)/2;
            return true;
    }
    return false;
}

} // namespace Gui
