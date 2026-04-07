#include "gui_tabs.hpp"
#include "gui_draw.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace Gui {

static const char* AIMBOT_MODE_NAMES[] = { "Off", "Legit", "Rage", "Triggerbot-only" };
static const char* AIMBOT_TARGET_PRIORITIES[] = { "Closest to crosshair", "Closest to player", "Lowest HP", "Visible only", "FOV priority" };
static const char* AIMBOT_ACCEL_CURVES[] = { "Linear", "Ease-in", "Ease-out", "Sigmoid" };
static const char* AIMBOT_VISIBILITY_OPTIONS[] = { "Simple screen visibility", "Ray-trace visibility" };
static const char* AIM_BONE_LABELS[] = { "Head", "Neck", "Chest", "Stomach", "Pelvis", "Arms", "Legs" };
static const int   AIM_BONE_GROUPS[7][6] = {
    { 6, -1, -1, -1, -1, -1 },
    { 5, -1, -1, -1, -1, -1 },
    { 4, -1, -1, -1, -1, -1 },
    { 2, -1, -1, -1, -1, -1 },
    { 0, -1, -1, -1, -1, -1 },
    { 8, 9, 11, 13, 14, 16 },
    { 22, 23, 24, 25, 26, 27 }
};

static void fillBonePriorityList(HWND lb, const Gui::VisualConfig& vis) {
    SendMessage(lb, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < 7; ++i) {
        int cat = vis.bonePriorityOrder[i];
        const char* label = AIM_BONE_LABELS[cat];
        SendMessage(lb, LB_ADDSTRING, 0, (LPARAM)label);
    }
    SendMessage(lb, LB_SETCURSEL, 0, 0);
}

static const char* AIMBOT_SECTION_NAMES[] = { "General", "Behavior", "Performance", "Visual" };
static const char* ESP_MODE_NAMES[] = { "Advanced Chams", "Bone ESP", "Perfect Flat Chams" };
static HWND s_aimSectionBtns[4]   = {};
static HWND s_aimSectionPanels[4] = {};
static int  s_activeAimbotSection = 0;

static void showAimbotSection(int idx) {
    if (idx < 0 || idx >= 4) return;
    s_activeAimbotSection = idx;
    for (int i = 0; i < 4; ++i) {
        if (s_aimSectionPanels[i]) ShowWindow(s_aimSectionPanels[i], i == idx ? SW_SHOW : SW_HIDE);
        if (s_aimSectionBtns[i]) SendMessage(s_aimSectionBtns[i], BM_SETSTATE, i == idx ? TRUE : FALSE, 0);
    }
}

static HWND findAimbotControl(int id) {
    for (HWND panel : s_aimSectionPanels) {
        if (!panel) continue;
        HWND ctrl = GetDlgItem(panel, id);
        if (ctrl) return ctrl;
    }
    return nullptr;
}

static const char* getAimbotText(int id) {
    static char buf[32];
    HWND ctrl = findAimbotControl(id);
    if (ctrl) GetWindowTextA(ctrl, buf, 32);
    else buf[0] = '\0';
    return buf;
}

static void buildCombatGeneral(HWND p) {
    int x = 10, y = 10;
    mkHeader(p, "General", x, y); y += 26;

    HWND cbEnabled = CreateWindowA("BUTTON", "Aimbot enabled",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_ENABLED, s_hInst, nullptr);
    SendMessage(cbEnabled, BM_SETCHECK, s_visuals.aimbotEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    mkLabel(p, "Mode", x, y, 100, 16);
    HWND cbMode = CreateWindowA("COMBOBOX", "",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        x+132, y-2, 180, 120,
        p, (HMENU)(intptr_t)ID_AIM_MODE, s_hInst, nullptr);
    for (auto name : AIMBOT_MODE_NAMES) SendMessage(cbMode, CB_ADDSTRING, 0, (LPARAM)name);
    SendMessage(cbMode, CB_SETCURSEL, s_visuals.aimbotMode, 0);
    y += 34;

    mkLabel(p, "Target priority", x, y, 110, 16);
    HWND cbPriority = CreateWindowA("COMBOBOX", "",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        x+132, y-2, 220, 120,
        p, (HMENU)(intptr_t)ID_AIM_TARGET_PRIORITY, s_hInst, nullptr);
    for (auto name : AIMBOT_TARGET_PRIORITIES) SendMessage(cbPriority, CB_ADDSTRING, 0, (LPARAM)name);
    SendMessage(cbPriority, CB_SETCURSEL, s_visuals.targetPriority, 0);
    y += 34;

    HWND cbVisibleOnly = CreateWindowA("BUTTON", "Only visible targets",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_VISIBLE_ONLY, s_hInst, nullptr);
    SendMessage(cbVisibleOnly, BM_SETCHECK, s_visuals.visibleOnly ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    mkLabel(p, "Max FOV", x, y, 100, 16);
    HWND sbFov = mkSlider(p, x+132, y, 180, 22, ID_VIS_MAX_FOV, 0, 180, s_visuals.aimMaxFov);
    y += 34;

    mkLabel(p, "Max distance", x, y, 100, 16);
    char bufMaxDist[32]; sprintf_s(bufMaxDist, "%d", s_visuals.maxDistance);
    mkEdit(p, bufMaxDist, x+132, y-2, 80, 22, ID_AIM_MAX_DIST, false, false);
    y += 34;

    HWND cbIgnoreTeam = CreateWindowA("BUTTON", "Ignore teammates",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_IGNORE_TEAM, s_hInst, nullptr);
    SendMessage(cbIgnoreTeam, BM_SETCHECK, s_visuals.ignoreTeammates ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbIgnoreFlash = CreateWindowA("BUTTON", "Ignore flashed enemies",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_IGNORE_FLASH, s_hInst, nullptr);
    SendMessage(cbIgnoreFlash, BM_SETCHECK, s_visuals.ignoreFlashed ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbIgnoreSmoke = CreateWindowA("BUTTON", "Ignore smoke targets",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_IGNORE_SMOKE, s_hInst, nullptr);
    SendMessage(cbIgnoreSmoke, BM_SETCHECK, s_visuals.ignoreSmoke ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbIgnoreScoped = CreateWindowA("BUTTON", "Only while scoped",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_IGNORE_SCOPED, s_hInst, nullptr);
    SendMessage(cbIgnoreScoped, BM_SETCHECK, s_visuals.ignoreScopedOnly ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    mkLabel(p, "Target switch delay (ms)", x, y, 140, 16);
    char bufSwitch[32]; sprintf_s(bufSwitch, "%d", s_visuals.targetSwitchMs);
    mkEdit(p, bufSwitch, x+150, y-2, 60, 22, ID_AIM_SWITCH_DELAY, false, false);

    RECT rc; GetClientRect(p, &rc);
    int contentHeight = y + 28;
}

static void buildCombatBehavior(HWND p) {
    int x = 10, y = 10;
    mkHeader(p, "Behavior", x, y); y += 26;

    mkLabel(p, "Smoothness", x, y, 120, 16);
    mkSlider(p, x+132, y, 180, 22, ID_AIM_SMOOTHNESS, 0, 100, (int)(s_visuals.smoothness * 100.0f));
    y += 34;

    mkLabel(p, "Acceleration curve", x, y, 130, 16);
    HWND cbCurve = CreateWindowA("COMBOBOX", "",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        x+150, y-2, 160, 120,
        p, (HMENU)(intptr_t)ID_AIM_ACCEL_CURVE, s_hInst, nullptr);
    for (auto name : AIMBOT_ACCEL_CURVES) SendMessage(cbCurve, CB_ADDSTRING, 0, (LPARAM)name);
    SendMessage(cbCurve, CB_SETCURSEL, s_visuals.accelCurve, 0);
    y += 34;

    mkLabel(p, "Accel strength", x, y, 120, 16);
    char bufAccel[32]; sprintf_s(bufAccel, "%.2f", s_visuals.accelStrength);
    mkEdit(p, bufAccel, x+132, y-2, 80, 22, ID_AIM_ACCEL_STRENGTH, false, false);
    y += 34;

    mkLabel(p, "Randomization", x, y, 120, 16);
    mkSlider(p, x+132, y, 180, 22, ID_AIM_RANDOMIZATION, 0, 100, (int)(s_visuals.randomization * 100.0f));
    y += 34;

    HWND cbRecoil = CreateWindowA("BUTTON", "Recoil compensation",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_RECOIL, s_hInst, nullptr);
    SendMessage(cbRecoil, BM_SETCHECK, s_visuals.recoilCompensation ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    mkLabel(p, "Recoil strength", x, y, 120, 16);
    char bufRecoil[32]; sprintf_s(bufRecoil, "%.2f", s_visuals.recoilStrength);
    mkEdit(p, bufRecoil, x+132, y-2, 80, 22, ID_AIM_RECOIL_STRENGTH, false, false);
    y += 34;

    HWND cbSilent = CreateWindowA("BUTTON", "Silent aim",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_SILENT, s_hInst, nullptr);
    SendMessage(cbSilent, BM_SETCHECK, s_visuals.silentAim ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbAutoShoot = CreateWindowA("BUTTON", "Auto shoot",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_AUTO_SHOOT, s_hInst, nullptr);
    SendMessage(cbAutoShoot, BM_SETCHECK, s_visuals.autoShoot ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    mkLabel(p, "Minimum hitchance", x, y, 120, 16);
    mkSlider(p, x+132, y, 180, 22, ID_AIM_MIN_HITCHANCE, 0, 100, s_visuals.minHitChance);
    y += 34;

    HWND cbAutoScope = CreateWindowA("BUTTON", "Auto scope",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_AUTO_SCOPE, s_hInst, nullptr);
    SendMessage(cbAutoScope, BM_SETCHECK, s_visuals.autoScope ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbAutoStop = CreateWindowA("BUTTON", "Auto stop for rifles",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_AUTO_STOP, s_hInst, nullptr);
    SendMessage(cbAutoStop, BM_SETCHECK, s_visuals.autoStop ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    RECT rc; GetClientRect(p, &rc);
    int contentHeight = y + 28;
    setPanelScrollRange(p, contentHeight);
}

static void buildCombatPerformance(HWND p) {
    int x = 10, y = 10;
    mkHeader(p, "Performance", x, y); y += 26;

    mkLabel(p, "Update rate (ms)", x, y, 120, 16);
    char bufUpdate[32]; sprintf_s(bufUpdate, "%d", s_visuals.aimbotUpdateMs);
    mkEdit(p, bufUpdate, x+132, y-2, 80, 22, ID_AIM_UPDATE_RATE, false, false);
    y += 34;

    HWND cbPrediction = CreateWindowA("BUTTON", "Enemy movement prediction",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_PREDICTION, s_hInst, nullptr);
    SendMessage(cbPrediction, BM_SETCHECK, s_visuals.predictionEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbResolver = CreateWindowA("BUTTON", "Basic anti-anti-aim resolver",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_RESOLVER, s_hInst, nullptr);
    SendMessage(cbResolver, BM_SETCHECK, s_visuals.resolverEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    mkLabel(p, "Resolver strength", x, y, 120, 16);
    char bufResolve[32]; sprintf_s(bufResolve, "%d", s_visuals.resolverStrength);
    mkEdit(p, bufResolve, x+132, y-2, 80, 22, ID_AIM_RESOLVER_STRENGTH, false, false);
    y += 34;

    mkLabel(p, "Visibility check", x, y, 120, 16);
    HWND cbVisCheck = CreateWindowA("COMBOBOX", "",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        x+132, y-2, 180, 120,
        p, (HMENU)(intptr_t)ID_AIM_VISIBILITY_CHECK, s_hInst, nullptr);
    for (auto name : AIMBOT_VISIBILITY_OPTIONS) SendMessage(cbVisCheck, CB_ADDSTRING, 0, (LPARAM)name);
    SendMessage(cbVisCheck, CB_SETCURSEL, s_visuals.visibilityRayTrace ? 1 : 0, 0);
    y += 34;

    mkLabel(p, "Min reaction (ms)", x, y, 120, 16);
    char bufReact[32]; sprintf_s(bufReact, "%d", s_visuals.minReactionMs);
    mkEdit(p, bufReact, x+132, y-2, 80, 22, ID_AIM_MIN_REACT, false, false);
}

static void buildCombatVisual(HWND p) {
    int x = 10, y = 10;
    mkHeader(p, "Visual", x, y); y += 26;

    HWND cbFovCircle = CreateWindowA("BUTTON", "Draw FOV circle",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_DRAW_FOV, s_hInst, nullptr);
    SendMessage(cbFovCircle, BM_SETCHECK, s_visuals.drawFovCircle ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    mkLabel(p, "FOV thickness", x, y, 120, 16);
    mkSlider(p, x+132, y, 180, 22, ID_AIM_FOV_THICKNESS, 1, 6, s_visuals.fovCircleThickness);
    y += 34;

    HWND cbDrawLine = CreateWindowA("BUTTON", "Draw target line",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_DRAW_LINE, s_hInst, nullptr);
    SendMessage(cbDrawLine, BM_SETCHECK, s_visuals.drawTargetLine ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbDrawDot = CreateWindowA("BUTTON", "Draw target dot",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_DRAW_DOT, s_hInst, nullptr);
    SendMessage(cbDrawDot, BM_SETCHECK, s_visuals.drawTargetDot ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbDrawPoint = CreateWindowA("BUTTON", "Draw current aim point",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 260, 22,
        p, (HMENU)(intptr_t)ID_AIM_DRAW_POINT, s_hInst, nullptr);
    SendMessage(cbDrawPoint, BM_SETCHECK, s_visuals.drawAimPoint ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;

    HWND cbShowInfo = CreateWindowA("BUTTON", "Show target info above player",
        WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x, y, 300, 22,
        p, (HMENU)(intptr_t)ID_AIM_SHOW_INFO, s_hInst, nullptr);
    SendMessage(cbShowInfo, BM_SETCHECK, s_visuals.showTargetInfo ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 32;

    mkHeader(p, "Bone priority", x, y); y += 24;
    HWND lbBones = CreateWindowA("LISTBOX", "",
        WS_CHILD|WS_VISIBLE|LBS_NOTIFY|LBS_STANDARD,
        x, y, 180, 180,
        p, (HMENU)(intptr_t)ID_AIM_BONE_LIST, s_hInst, nullptr);
    fillBonePriorityList(lbBones, s_visuals);

    int checkY = y;
    for (int i = 0; i < 7; ++i) {
        HWND cbBone = CreateWindowA("BUTTON", AIM_BONE_LABELS[i],
            WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
            x+200, checkY + i*24, 120, 22,
            p, (HMENU)(intptr_t)(ID_AIM_BONE_ENABLE_BASE + i), s_hInst, nullptr);
        SendMessage(cbBone, BM_SETCHECK, s_visuals.bonePriorityEnabled[i] ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    HWND btnBoneUp = mkButton(p, "Move Up", x+200, y+180, 80, 28, ID_AIM_BONE_UP);
    HWND btnBoneDown = mkButton(p, "Move Down", x+290, y+180, 80, 28, ID_AIM_BONE_DOWN);

    int btnX = x + 430;
    mkButton(p, "Load Legit", btnX, y+10, 120, 28, ID_AIM_PRESET_LEGIT);
    mkButton(p, "Load Rage", btnX, y+50, 120, 28, ID_AIM_PRESET_RAGE);
    mkButton(p, "Save Custom", btnX, y+90, 120, 28, ID_AIM_PRESET_SAVE);

    RECT rc; GetClientRect(p, &rc);
    int contentHeight = y + 28;
}

void buildESP(HWND p) {
    int x = 10, y = 10;
    mkHeader(p, "ESP", x, y); y += 26;

    mkLabel(p, "ESP mode", x, y, 100, 16);
    HWND cbEspMode = CreateWindowA("COMBOBOX", "",
        WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,
        x+132, y-2, 220, 120,
        p, (HMENU)(intptr_t)ID_VIS_ESP_MODE, s_hInst, nullptr);
    for (auto name : ESP_MODE_NAMES) SendMessage(cbEspMode, CB_ADDSTRING, 0, (LPARAM)name);
    SendMessage(cbEspMode, CB_SETCURSEL, s_visuals.espMode, 0);
    y += 34;

    mkLabel(p, "ESP strength", x, y, 100, 16);
    mkSlider(p, x+132, y, 180, 22, ID_VIS_ESP_STRENGTH, 1, 5, s_visuals.espStrength);
    mkValLbl(p, s_visuals.espStrength, x+320, y, ID_VIS_ESP_STRENGTH_LBL);
    y += 42;

    int applyY = y;
    mkButton(p, "Apply ESP", x, applyY, 120, 28, ID_VIS_APPLY_ESP);
}

static void swapBonePriority(Gui::VisualConfig& vis, int idx, int dir) {
    if (idx < 0 || idx >= 7) return;
    int target = idx + dir;
    if (target < 0 || target >= 7) return;
    std::swap(vis.bonePriorityOrder[idx], vis.bonePriorityOrder[target]);
    std::swap(vis.bonePriorityEnabled[idx], vis.bonePriorityEnabled[target]);
}

static bool writeAimbotJson(const char* fileName, const Gui::VisualConfig& cfg) {
    std::ofstream out(fileName);
    if (!out) return false;
    out << "{\n";
    out << "  \"aimbotEnabled\": " << (cfg.aimbotEnabled ? "true" : "false") << ",\n";
    out << "  \"aimbotMode\": " << cfg.aimbotMode << ",\n";
    out << "  \"targetPriority\": " << cfg.targetPriority << ",\n";
    out << "  \"visibleOnly\": " << (cfg.visibleOnly ? "true" : "false") << ",\n";
    out << "  \"maxDistance\": " << cfg.maxDistance << ",\n";
    out << "  \"ignoreTeammates\": " << (cfg.ignoreTeammates ? "true" : "false") << ",\n";
    out << "  \"ignoreFlashed\": " << (cfg.ignoreFlashed ? "true" : "false") << ",\n";
    out << "  \"ignoreSmoke\": " << (cfg.ignoreSmoke ? "true" : "false") << ",\n";
    out << "  \"ignoreScopedOnly\": " << (cfg.ignoreScopedOnly ? "true" : "false") << ",\n";
    out << "  \"targetSwitchMs\": " << cfg.targetSwitchMs << ",\n";
    out << "  \"smoothness\": " << cfg.smoothness << ",\n";
    out << "  \"accelCurve\": " << cfg.accelCurve << ",\n";
    out << "  \"accelStrength\": " << cfg.accelStrength << ",\n";
    out << "  \"randomization\": " << cfg.randomization << ",\n";
    out << "  \"recoilCompensation\": " << (cfg.recoilCompensation ? "true" : "false") << ",\n";
    out << "  \"recoilStrength\": " << cfg.recoilStrength << ",\n";
    out << "  \"silentAim\": " << (cfg.silentAim ? "true" : "false") << ",\n";
    out << "  \"autoShoot\": " << (cfg.autoShoot ? "true" : "false") << ",\n";
    out << "  \"minHitChance\": " << cfg.minHitChance << ",\n";
    out << "  \"autoScope\": " << (cfg.autoScope ? "true" : "false") << ",\n";
    out << "  \"autoStop\": " << (cfg.autoStop ? "true" : "false") << ",\n";
    out << "  \"aimbotUpdateMs\": " << cfg.aimbotUpdateMs << ",\n";
    out << "  \"predictionEnabled\": " << (cfg.predictionEnabled ? "true" : "false") << ",\n";
    out << "  \"resolverEnabled\": " << (cfg.resolverEnabled ? "true" : "false") << ",\n";
    out << "  \"resolverStrength\": " << cfg.resolverStrength << ",\n";
    out << "  \"visibilityRayTrace\": " << (cfg.visibilityRayTrace ? "true" : "false") << ",\n";
    out << "  \"minReactionMs\": " << cfg.minReactionMs << ",\n";
    out << "  \"drawFovCircle\": " << (cfg.drawFovCircle ? "true" : "false") << ",\n";
    out << "  \"fovCircleColor\": " << cfg.fovCircleColor << ",\n";
    out << "  \"fovCircleThickness\": " << cfg.fovCircleThickness << ",\n";
    out << "  \"drawTargetLine\": " << (cfg.drawTargetLine ? "true" : "false") << ",\n";
    out << "  \"drawTargetDot\": " << (cfg.drawTargetDot ? "true" : "false") << ",\n";
    out << "  \"drawAimPoint\": " << (cfg.drawAimPoint ? "true" : "false") << ",\n";
    out << "  \"showTargetInfo\": " << (cfg.showTargetInfo ? "true" : "false") << ",\n";
    out << "  \"bonePriorityOrder\": [";
    for (int i = 0; i < 7; ++i) {
        out << cfg.bonePriorityOrder[i];
        if (i < 6) out << ", ";
    }
    out << "],\n";
    out << "  \"bonePriorityEnabled\": [";
    for (int i = 0; i < 7; ++i) {
        out << (cfg.bonePriorityEnabled[i] ? "true" : "false");
        if (i < 6) out << ", ";
    }
    out << "]\n";
    out << "}\n";
    return true;
}

static std::string trimJsonToken(const std::string& token) {
    std::string out;
    for (char c : token) {
        if (c == '"' || c == ',' || c == ':' || c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        out.push_back(c);
    }
    return out;
}

static bool readAimbotJson(const char* fileName, Gui::VisualConfig& cfg) {
    std::ifstream in(fileName);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = trimJsonToken(line.substr(0, colon));
        std::string val = trimJsonToken(line.substr(colon + 1));
        if (key == "aimbotEnabled") cfg.aimbotEnabled = (val == "true");
        else if (key == "aimbotMode") cfg.aimbotMode = std::atoi(val.c_str());
        else if (key == "targetPriority") cfg.targetPriority = std::atoi(val.c_str());
        else if (key == "visibleOnly") cfg.visibleOnly = (val == "true");
        else if (key == "maxDistance") cfg.maxDistance = std::atoi(val.c_str());
        else if (key == "ignoreTeammates") cfg.ignoreTeammates = (val == "true");
        else if (key == "ignoreFlashed") cfg.ignoreFlashed = (val == "true");
        else if (key == "ignoreSmoke") cfg.ignoreSmoke = (val == "true");
        else if (key == "ignoreScopedOnly") cfg.ignoreScopedOnly = (val == "true");
        else if (key == "targetSwitchMs") cfg.targetSwitchMs = std::atoi(val.c_str());
        else if (key == "smoothness") cfg.smoothness = std::atof(val.c_str());
        else if (key == "accelCurve") cfg.accelCurve = std::atoi(val.c_str());
        else if (key == "accelStrength") cfg.accelStrength = std::atof(val.c_str());
        else if (key == "randomization") cfg.randomization = std::atof(val.c_str());
        else if (key == "recoilCompensation") cfg.recoilCompensation = (val == "true");
        else if (key == "recoilStrength") cfg.recoilStrength = std::atof(val.c_str());
        else if (key == "silentAim") cfg.silentAim = (val == "true");
        else if (key == "autoShoot") cfg.autoShoot = (val == "true");
        else if (key == "minHitChance") cfg.minHitChance = std::atoi(val.c_str());
        else if (key == "autoScope") cfg.autoScope = (val == "true");
        else if (key == "autoStop") cfg.autoStop = (val == "true");
        else if (key == "aimbotUpdateMs") cfg.aimbotUpdateMs = std::atoi(val.c_str());
        else if (key == "predictionEnabled") cfg.predictionEnabled = (val == "true");
        else if (key == "resolverEnabled") cfg.resolverEnabled = (val == "true");
        else if (key == "resolverStrength") cfg.resolverStrength = std::atoi(val.c_str());
        else if (key == "visibilityRayTrace") cfg.visibilityRayTrace = (val == "true");
        else if (key == "minReactionMs") cfg.minReactionMs = std::atoi(val.c_str());
        else if (key == "drawFovCircle") cfg.drawFovCircle = (val == "true");
        else if (key == "fovCircleColor") cfg.fovCircleColor = static_cast<COLORREF>(std::stoul(val));
        else if (key == "fovCircleThickness") cfg.fovCircleThickness = std::atoi(val.c_str());
        else if (key == "drawTargetLine") cfg.drawTargetLine = (val == "true");
        else if (key == "drawTargetDot") cfg.drawTargetDot = (val == "true");
        else if (key == "drawAimPoint") cfg.drawAimPoint = (val == "true");
        else if (key == "showTargetInfo") cfg.showTargetInfo = (val == "true");
        else if (key == "bonePriorityOrder") {
            std::vector<int> values;
            std::stringstream ss(val);
            int num;
            while (ss >> num) {
                values.push_back(num);
                if (ss.peek() == ',') ss.ignore();
            }
            if (values.size() == 7) {
                for (int i = 0; i < 7; ++i) cfg.bonePriorityOrder[i] = values[i];
            }
        } else if (key == "bonePriorityEnabled") {
            std::vector<bool> values;
            for (size_t i = 0; i < val.size(); ++i) {
                if (val[i] == 't') values.push_back(true);
                else if (val[i] == 'f') values.push_back(false);
            }
            if (values.size() == 7) {
                for (int i = 0; i < 7; ++i) cfg.bonePriorityEnabled[i] = values[i];
            }
        }
    }
    return true;
}

static bool loadPresetConfig(const char* fileName) {
    Gui::VisualConfig vis = Gui::getVisuals();
    if (!readAimbotJson(fileName, vis)) return false;
    Gui::setVisuals(vis);
    return true;
}

static bool saveCustomPresetConfig(const char* fileName) {
    Gui::VisualConfig vis = Gui::getVisuals();
    return writeAimbotJson(fileName, vis);
}

void buildCombat(HWND p) {
    static constexpr const char* PANEL_CLASS = "OverlayPanel";
    int x = 18, y = 18;

    mkHeader(p, "Aimbot", x, y); y += 26;

    const int tabWidth = 118;
    for (int i = 0; i < 4; ++i) {
        s_aimSectionBtns[i] = CreateWindowA("BUTTON", AIMBOT_SECTION_NAMES[i],
            WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            x + i * tabWidth, y, tabWidth - 8, 28,
            p, (HMENU)(intptr_t)(ID_AIM_TAB_GENERAL + i), s_hInst, nullptr);
        SendMessage(s_aimSectionBtns[i], WM_SETFONT, (WPARAM)s_fUI, TRUE);
    }
    y += 42;

    RECT rc; GetClientRect(p, &rc);
    int panelW = 760;
    int panelH = std::max(1, static_cast<int>((rc.bottom - rc.top) - y - 80));
    if (panelH > 620) panelH = 620;

    for (int i = 0; i < 4; ++i) {
        s_aimSectionPanels[i] = CreateWindowA(PANEL_CLASS, "",
            WS_CHILD|WS_CLIPCHILDREN | (i == 0 ? WS_VISIBLE : 0),
            x, y, panelW, panelH,
            p, nullptr, s_hInst, nullptr);
    }

    buildCombatGeneral(s_aimSectionPanels[0]);
    buildCombatBehavior(s_aimSectionPanels[1]);
    buildCombatPerformance(s_aimSectionPanels[2]);
    buildCombatVisual(s_aimSectionPanels[3]);
    showAimbotSection(0);

    int applyY = y + panelH + 10;
    if (applyY + 28 > rc.bottom) applyY = rc.bottom - 38;
    mkButton(p, "Apply Aimbot", x, applyY, 120, 28, ID_VIS_APPLY);
}

bool handleESP(WPARAM wp) {
    switch (LOWORD(wp)) {
        case ID_VIS_APPLY_ESP: {
            s_visuals.espMode = (int)SendMessage(GetDlgItem(s_panels[2], ID_VIS_ESP_MODE), CB_GETCURSEL, 0, 0);
            s_visuals.espStrength = (int)SendMessage(GetDlgItem(s_panels[2], ID_VIS_ESP_STRENGTH), TBM_GETPOS, 0, 0);
            return true;
        }
        default:
            return false;
    }
}

bool handleCombat(WPARAM wp) {
    switch (LOWORD(wp)) {
        case ID_AIM_TAB_GENERAL:
        case ID_AIM_TAB_BEHAVIOR:
        case ID_AIM_TAB_PERFORMANCE:
        case ID_AIM_TAB_VISUAL: {
            showAimbotSection(LOWORD(wp) - ID_AIM_TAB_GENERAL);
            return true;
        }
        case ID_VIS_APPLY: {
            s_visuals.aimbotEnabled = (SendMessage(findAimbotControl(ID_AIM_ENABLED), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.aimbotMode = (int)SendMessage(findAimbotControl(ID_AIM_MODE), CB_GETCURSEL, 0, 0);
            s_visuals.targetPriority = (int)SendMessage(findAimbotControl(ID_AIM_TARGET_PRIORITY), CB_GETCURSEL, 0, 0);
            s_visuals.visibleOnly = (SendMessage(findAimbotControl(ID_AIM_VISIBLE_ONLY), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.maxDistance = atoi(getAimbotText(ID_AIM_MAX_DIST));
            s_visuals.ignoreTeammates = (SendMessage(findAimbotControl(ID_AIM_IGNORE_TEAM), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.ignoreFlashed = (SendMessage(findAimbotControl(ID_AIM_IGNORE_FLASH), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.ignoreSmoke = (SendMessage(findAimbotControl(ID_AIM_IGNORE_SMOKE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.ignoreScopedOnly = (SendMessage(findAimbotControl(ID_AIM_IGNORE_SCOPED), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.targetSwitchMs = atoi(getAimbotText(ID_AIM_SWITCH_DELAY));
            s_visuals.smoothness = (float)SendMessage(findAimbotControl(ID_AIM_SMOOTHNESS), TBM_GETPOS, 0, 0) / 100.0f;
            if (s_visuals.smoothness < 0.0f) s_visuals.smoothness = 0.0f;
            if (s_visuals.smoothness > 1.0f) s_visuals.smoothness = 1.0f;
            s_visuals.accelCurve = (int)SendMessage(findAimbotControl(ID_AIM_ACCEL_CURVE), CB_GETCURSEL, 0, 0);
            {
                char buf[32]; GetWindowTextA(findAimbotControl(ID_AIM_ACCEL_STRENGTH), buf, 32);
                s_visuals.accelStrength = static_cast<float>(atof(buf));
                if (s_visuals.accelStrength < 0.0f) s_visuals.accelStrength = 0.0f;
                if (s_visuals.accelStrength > 1.0f) s_visuals.accelStrength = 1.0f;
            }
            s_visuals.randomization = SendMessage(findAimbotControl(ID_AIM_RANDOMIZATION), TBM_GETPOS, 0, 0) / 100.0f;
            s_visuals.recoilCompensation = (SendMessage(findAimbotControl(ID_AIM_RECOIL), BM_GETCHECK, 0, 0) == BST_CHECKED);
            {
                char buf[32]; GetWindowTextA(findAimbotControl(ID_AIM_RECOIL_STRENGTH), buf, 32);
                s_visuals.recoilStrength = static_cast<float>(atof(buf));
                if (s_visuals.recoilStrength < 0.0f) s_visuals.recoilStrength = 0.0f;
                if (s_visuals.recoilStrength > 1.0f) s_visuals.recoilStrength = 1.0f;
            }
            s_visuals.silentAim = (SendMessage(findAimbotControl(ID_AIM_SILENT), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.autoShoot = (SendMessage(findAimbotControl(ID_AIM_AUTO_SHOOT), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.minHitChance = (int)SendMessage(findAimbotControl(ID_AIM_MIN_HITCHANCE), TBM_GETPOS, 0, 0);
            s_visuals.autoScope = (SendMessage(findAimbotControl(ID_AIM_AUTO_SCOPE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.autoStop = (SendMessage(findAimbotControl(ID_AIM_AUTO_STOP), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.aimbotUpdateMs = atoi(getAimbotText(ID_AIM_UPDATE_RATE));
            s_visuals.predictionEnabled = (SendMessage(findAimbotControl(ID_AIM_PREDICTION), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.resolverEnabled = (SendMessage(findAimbotControl(ID_AIM_RESOLVER), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.resolverStrength = atoi(getAimbotText(ID_AIM_RESOLVER_STRENGTH));
            s_visuals.visibilityRayTrace = (SendMessage(findAimbotControl(ID_AIM_VISIBILITY_CHECK), CB_GETCURSEL, 0, 0) == 1);
            s_visuals.minReactionMs = atoi(getAimbotText(ID_AIM_MIN_REACT));
            s_visuals.drawFovCircle = (SendMessage(findAimbotControl(ID_AIM_DRAW_FOV), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.fovCircleThickness = (int)SendMessage(findAimbotControl(ID_AIM_FOV_THICKNESS), TBM_GETPOS, 0, 0);
            s_visuals.drawTargetLine = (SendMessage(findAimbotControl(ID_AIM_DRAW_LINE), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.drawTargetDot = (SendMessage(findAimbotControl(ID_AIM_DRAW_DOT), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.drawAimPoint = (SendMessage(findAimbotControl(ID_AIM_DRAW_POINT), BM_GETCHECK, 0, 0) == BST_CHECKED);
            s_visuals.showTargetInfo = (SendMessage(findAimbotControl(ID_AIM_SHOW_INFO), BM_GETCHECK, 0, 0) == BST_CHECKED);
            for (int i = 0; i < 7; ++i) {
                s_visuals.bonePriorityEnabled[i] = (SendMessage(findAimbotControl(ID_AIM_BONE_ENABLE_BASE + i), BM_GETCHECK, 0, 0) == BST_CHECKED);
            }
            return true;
        }
        case ID_AIM_PRESET_LEGIT: {
            loadPresetConfig("aimbot_preset_legit.json");
            return true;
        }
        case ID_AIM_PRESET_RAGE: {
            loadPresetConfig("aimbot_preset_rage.json");
            return true;
        }
        case ID_AIM_PRESET_SAVE: {
            saveCustomPresetConfig("aimbot_preset_custom.json");
            return true;
        }
        case ID_AIM_BONE_UP:
        case ID_AIM_BONE_DOWN: {
            HWND lb = findAimbotControl(ID_AIM_BONE_LIST);
            if (!lb) return true;
            int sel = (int)SendMessage(lb, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                Gui::VisualConfig vis = Gui::getVisuals();
                swapBonePriority(vis, sel, LOWORD(wp) == ID_AIM_BONE_UP ? -1 : 1);
                Gui::setVisuals(vis);
                fillBonePriorityList(lb, vis);
                SendMessage(lb, LB_SETCURSEL, sel + (LOWORD(wp) == ID_AIM_BONE_UP ? -1 : 1), 0);
            }
            return true;
        }
    }
    return false;
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
