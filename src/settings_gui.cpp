#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "settings_gui.h"
#include "config.h"
#include "overlay.h"
#include "vignette.h"
#include "dota_gsi.h"
#include "resource.h"
#include "updater.h"
#include "voice_assistant.h"
#include "voice.h"
#include "hero_advisor.h"
#include <algorithm>
#include <commctrl.h>
#include <uxtheme.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

using namespace Gdiplus;

static const wchar_t* SETTINGS_CLASS = L"BadPiggiesSettingsClass";

enum ControlIDs {
    IDC_COMBO_PIGS = 1001,
    IDC_COMBO_SIZE = 1002,
    IDC_CHK_SOUND = 1003,
    IDC_CHK_HIDE = 1004,
    IDC_CHK_AUTOSTART = 1005,
    IDC_BTN_TEST_KILL = 1006,
    IDC_BTN_TEST_DEATH = 1007,
    IDC_BTN_SETUP_DOTA = 1008,
    IDC_BTN_SAVE = 1009,
    IDC_PREVIEW = 1010,
    IDC_LBL_STATUS = 1011,
    IDC_BTN_TEST_LOW_HP = 1012,
    IDC_CHK_USE_NAME = 1013,
    IDC_TXT_PLAYER_NAME = 1014,
    IDC_CHK_VOICE_TTS = 1015,
    IDC_BTN_CHECK_UPDATE = 1016,
    IDC_CHK_VOICE_ASSISTANT = 1017,
    IDC_TXT_WAKE_WORD = 1018,
    IDC_CHK_PROACTIVE_ADVISOR = 1019,
    IDC_BTN_TEST_ADVISOR = 1020
};

SettingsWindow& SettingsWindow::Instance() {
    static SettingsWindow instance;
    return instance;
}

SettingsWindow::SettingsWindow() {}

SettingsWindow::~SettingsWindow() {
    if (hFont_) DeleteObject(hFont_);
    if (hFontBold_) DeleteObject(hFontBold_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool SettingsWindow::Create() {
    INITCOMMONCONTROLSEX icx = { 0 };
    icx.dwSize = sizeof(icx);
    icx.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icx);

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = SettingsWindow::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = SETTINGS_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);

    HICON hAppIcon = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!hAppIcon) {
        hAppIcon = (HICON)LoadImageW(NULL, ConfigManager::GetAssetPath(L"app.ico").c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    }
    if (!hAppIcon) hAppIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIcon = hAppIcon;
    RegisterClassExW(&wc);

    hFont_ = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    hFontBold_ = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int winW = 600;
    int winH = 760;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        SETTINGS_CLASS,
        L"Bad Piggies Widget — Настройки",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, winW, winH,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    if (!hwnd_) return false;

    SendMessageW(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)hAppIcon);
    SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)hAppIcon);

    CreateControls();
    LoadCurrentSettingsToUI();
    return true;
}

void SettingsWindow::CreateControls() {
    HINSTANCE hInst = GetModuleHandle(NULL);

    // Group 1: Choose Pig
    HWND grpPig = CreateWindowW(L"BUTTON", L" Выбор свинки ",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 15, 310, 195, hwnd_, NULL, hInst, NULL);
    SendMessageW(grpPig, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    HWND lblPig = CreateWindowW(L"STATIC", L"Внешний вид свинки:",
        WS_CHILD | WS_VISIBLE,
        35, 45, 275, 20, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblPig, WM_SETFONT, (WPARAM)hFont_, TRUE);

    comboPigs_ = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        35, 70, 275, 260, hwnd_, (HMENU)IDC_COMBO_PIGS, hInst, NULL);
    SendMessageW(comboPigs_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    const wchar_t* pigNames[] = {
        L"Вид 1 (Король свиней с короной)",
        L"Вид 2 (3D Свинка из фильма)",
        L"Вид 3 (Классическая свинка)",
        L"Вид 4 (Thug Life в очках)",
        L"Вид 5 (Свинка в кепке)",
        L"Вид 6 (Свинка с веснушками)",
        L"Вид 7 (Улыбающаяся свинка)"
    };

    for (int i = 0; i < 7; ++i) {
        SendMessageW(comboPigs_, CB_ADDSTRING, 0, (LPARAM)pigNames[i]);
    }

    HWND lblSize = CreateWindowW(L"STATIC", L"Размер виджета на экране:",
        WS_CHILD | WS_VISIBLE,
        35, 120, 275, 20, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblSize, WM_SETFONT, (WPARAM)hFont_, TRUE);

    comboSize_ = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        35, 145, 275, 150, hwnd_, (HMENU)IDC_COMBO_SIZE, hInst, NULL);
    SendMessageW(comboSize_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    SendMessageW(comboSize_, CB_ADDSTRING, 0, (LPARAM)L"Маленький (130 px)");
    SendMessageW(comboSize_, CB_ADDSTRING, 0, (LPARAM)L"Средний (180 px - Рекомендуется)");
    SendMessageW(comboSize_, CB_ADDSTRING, 0, (LPARAM)L"Большой (240 px)");

    // Group 2: Preview
    HWND grpPreview = CreateWindowW(L"BUTTON", L" Превью ",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        345, 15, 225, 195, hwnd_, NULL, hInst, NULL);
    SendMessageW(grpPreview, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    previewBox_ = CreateWindowW(L"STATIC", NULL,
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        360, 42, 195, 155, hwnd_, (HMENU)IDC_PREVIEW, hInst, NULL);

    // Group 3: Options & Assistant
    HWND grpOptions = CreateWindowW(L"BUTTON", L" Параметры и Помощник ",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 215, 310, 420, hwnd_, NULL, hInst, NULL);
    SendMessageW(grpOptions, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    chkSound_ = CreateWindowW(L"BUTTON", L"Звуковые эффекты (WAV)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 238, 280, 20, hwnd_, (HMENU)IDC_CHK_SOUND, hInst, NULL);
    SendMessageW(chkSound_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkVoiceTTS_ = CreateWindowW(L"BUTTON", L"Озвучка фраз:",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 260, 115, 20, hwnd_, (HMENU)IDC_CHK_VOICE_TTS, hInst, NULL);
    SendMessageW(chkVoiceTTS_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    comboVoiceGender_ = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        155, 258, 160, 120, hwnd_, NULL, hInst, NULL);
    SendMessageW(comboVoiceGender_, WM_SETFONT, (WPARAM)hFont_, TRUE);
    SendMessageW(comboVoiceGender_, CB_ADDSTRING, 0, (LPARAM)L"Женский (Ирина)");
    SendMessageW(comboVoiceGender_, CB_ADDSTRING, 0, (LPARAM)L"Мужской (Павел)");

    chkPigShadow_ = CreateWindowW(L"BUTTON", L"Объёмная 3D-тень свина",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 283, 280, 20, hwnd_, NULL, hInst, NULL);
    SendMessageW(chkPigShadow_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkDynamicIsland_ = CreateWindowW(L"BUTTON", L"Режим Dynamic Island (вверху)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 305, 280, 20, hwnd_, NULL, hInst, NULL);
    SendMessageW(chkDynamicIsland_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    chkHide_ = CreateWindowW(L"BUTTON", L"Скрывать с экрана, если Дота закрыта",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 328, 280, 20, hwnd_, (HMENU)IDC_CHK_HIDE, hInst, NULL);
    SendMessageW(chkHide_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkAutostart_ = CreateWindowW(L"BUTTON", L"Запускать вместе с Windows",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 350, 280, 20, hwnd_, (HMENU)IDC_CHK_AUTOSTART, hInst, NULL);
    SendMessageW(chkAutostart_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkUseName_ = CreateWindowW(L"BUTTON", L"Обращаться к вам:",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 375, 155, 20, hwnd_, (HMENU)IDC_CHK_USE_NAME, hInst, NULL);
    SendMessageW(chkUseName_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    txtPlayerName_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Марат",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        195, 374, 120, 22, hwnd_, (HMENU)IDC_TXT_PLAYER_NAME, hInst, NULL);
    SendMessageW(txtPlayerName_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    // Voice Assistant Controls
    chkVoiceAssistant_ = CreateWindowW(L"BUTTON", L"Голосовой помощник (микрофон)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 404, 280, 22, hwnd_, (HMENU)IDC_CHK_VOICE_ASSISTANT, hInst, NULL);
    SendMessageW(chkVoiceAssistant_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    HWND lblMic = CreateWindowW(L"STATIC", L"Микрофон:",
        WS_CHILD | WS_VISIBLE,
        35, 432, 80, 20, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblMic, WM_SETFONT, (WPARAM)hFont_, TRUE);

    comboMicrophone_ = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        120, 430, 195, 160, hwnd_, NULL, hInst, NULL);
    SendMessageW(comboMicrophone_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    HWND lblWake = CreateWindowW(L"STATIC", L"Имя помощника:",
        WS_CHILD | WS_VISIBLE,
        35, 460, 150, 20, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblWake, WM_SETFONT, (WPARAM)hFont_, TRUE);

    txtWakeWord_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Свин",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        195, 458, 120, 22, hwnd_, (HMENU)IDC_TXT_WAKE_WORD, hInst, NULL);
    SendMessageW(txtWakeWord_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkToxicMode_ = CreateWindowW(L"BUTTON", L"Говорить наравне (токсик / прожарка)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 488, 280, 22, hwnd_, NULL, hInst, NULL);
    SendMessageW(chkToxicMode_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    chkProactiveAdvisor_ = CreateWindowW(L"BUTTON", L"Советы по игре (закуп и скиллы)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 514, 280, 22, hwnd_, (HMENU)IDC_CHK_PROACTIVE_ADVISOR, hInst, NULL);
    SendMessageW(chkProactiveAdvisor_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    HWND lblAdvNote = CreateWindowW(L"STATIC", L"(Анализ героя, тайминги, инвентарь)",
        WS_CHILD | WS_VISIBLE,
        35, 538, 280, 16, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblAdvNote, WM_SETFONT, (WPARAM)hFont_, TRUE);

    // Group 4: Testing & Updates
    HWND grpTest = CreateWindowW(L"BUTTON", L" Тестирование и Обновление ",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        345, 215, 225, 420, hwnd_, NULL, hInst, NULL);
    SendMessageW(grpTest, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    btnTestKill_ = CreateWindowW(L"BUTTON", L"Тест: Килл! (Прыжок)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 245, 195, 28, hwnd_, (HMENU)IDC_BTN_TEST_KILL, hInst, NULL);
    SendMessageW(btnTestKill_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    btnTestDeath_ = CreateWindowW(L"BUTTON", L"Тест: Смерть",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 278, 195, 28, hwnd_, (HMENU)IDC_BTN_TEST_DEATH, hInst, NULL);
    SendMessageW(btnTestDeath_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    btnTestLowHp_ = CreateWindowW(L"BUTTON", L"Тест: Low HP (Виньетка)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 311, 195, 28, hwnd_, (HMENU)IDC_BTN_TEST_LOW_HP, hInst, NULL);
    SendMessageW(btnTestLowHp_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    btnTestAdvisor_ = CreateWindowW(L"BUTTON", L"🧠 Тест: Свин, что купить?",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 345, 195, 34, hwnd_, (HMENU)IDC_BTN_TEST_ADVISOR, hInst, NULL);
    SendMessageW(btnTestAdvisor_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    btnCheckUpdate_ = CreateWindowW(L"BUTTON", L"Проверить обновления",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 386, 195, 28, hwnd_, (HMENU)IDC_BTN_CHECK_UPDATE, hInst, NULL);
    SendMessageW(btnCheckUpdate_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    std::string vStr = ConfigManager::Get().appVersion;
    std::wstring verStr = L"Версия: v" + std::wstring(vStr.begin(), vStr.end()) + L" (Dynamic Island & AI)";
    HWND lblVer = CreateWindowW(L"STATIC", verStr.c_str(),
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        350, 420, 215, 18, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblVer, WM_SETFONT, (WPARAM)hFont_, TRUE);

    btnSetupDota_ = CreateWindowW(L"BUTTON", L"Обновить CFG Доты",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        360, 444, 195, 30, hwnd_, (HMENU)IDC_BTN_SETUP_DOTA, hInst, NULL);
    SendMessageW(btnSetupDota_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    // Status Label
    lblStatus_ = CreateWindowW(L"STATIC", L"Статус: поиск Dota 2...",
        WS_CHILD | WS_VISIBLE,
        25, 645, 545, 22, hwnd_, (HMENU)IDC_LBL_STATUS, hInst, NULL);
    SendMessageW(lblStatus_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    // Save button
    btnSave_ = CreateWindowW(L"BUTTON", L"Сохранить и применить",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        175, 672, 250, 44, hwnd_, (HMENU)IDC_BTN_SAVE, hInst, NULL);
    SendMessageW(btnSave_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);
}

void SettingsWindow::LoadCurrentSettingsToUI() {
    const AppConfig& cfg = ConfigManager::Get();

    SendMessageW(comboPigs_, CB_SETCURSEL, cfg.pigType - 1, 0);

    if (cfg.pigSize <= 140) SendMessageW(comboSize_, CB_SETCURSEL, 0, 0);
    else if (cfg.pigSize >= 210) SendMessageW(comboSize_, CB_SETCURSEL, 2, 0);
    else SendMessageW(comboSize_, CB_SETCURSEL, 1, 0);

    SendMessageW(chkSound_, BM_SETCHECK, cfg.enableSound ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkVoiceTTS_, BM_SETCHECK, cfg.enableVoiceTTS ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkPigShadow_, BM_SETCHECK, cfg.enablePigShadow ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkDynamicIsland_, BM_SETCHECK, cfg.enableDynamicIsland ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkHide_, BM_SETCHECK, cfg.hideWhenNoDota ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkAutostart_, BM_SETCHECK, cfg.autostart ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkUseName_, BM_SETCHECK, cfg.usePlayerName ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(txtPlayerName_, cfg.playerName.c_str());
    EnableWindow(txtPlayerName_, cfg.usePlayerName ? TRUE : FALSE);

    // Microphones list
    SendMessageW(comboMicrophone_, CB_RESETCONTENT, 0, 0);
    SendMessageW(comboMicrophone_, CB_ADDSTRING, 0, (LPARAM)L"По умолчанию (Системный)");
    auto mics = VoiceAssistant::GetAvailableMicrophones();
    int selMicIdx = 0;
    for (size_t i = 0; i < mics.size(); ++i) {
        SendMessageW(comboMicrophone_, CB_ADDSTRING, 0, (LPARAM)mics[i].first.c_str());
        if (!cfg.microphoneDevice.empty() && (cfg.microphoneDevice == mics[i].first || cfg.microphoneDevice == mics[i].second)) {
            selMicIdx = (int)i + 1;
        }
    }
    SendMessageW(comboMicrophone_, CB_SETCURSEL, selMicIdx, 0);

    SendMessageW(chkVoiceAssistant_, BM_SETCHECK, cfg.enableVoiceAssistant ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(txtWakeWord_, cfg.assistantWakeWord.c_str());
    EnableWindow(txtWakeWord_, cfg.enableVoiceAssistant ? TRUE : FALSE);

    SendMessageW(comboVoiceGender_, CB_SETCURSEL, cfg.voiceGender == 0 ? 0 : 1, 0);
    SendMessageW(chkToxicMode_, BM_SETCHECK, cfg.enableToxicMode ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(chkProactiveAdvisor_, BM_SETCHECK, cfg.enableProactiveAdvisor ? BST_CHECKED : BST_UNCHECKED, 0);

    OnPigSelectionChanged();
}

void SettingsWindow::OnPigSelectionChanged() {
    int idx = (int)SendMessageW(comboPigs_, CB_GETCURSEL, 0, 0);
    if (idx < 0) idx = 0;
    previewPigType_ = idx + 1;

    std::wstring imgFile = L"pig_" + std::to_wstring(previewPigType_) + L".png";
    std::wstring fullPath = ConfigManager::GetAssetPath(imgFile);
    previewImage_ = std::make_unique<Image>(fullPath.c_str());

    InvalidateRect(previewBox_, NULL, TRUE);
}

void SettingsWindow::ApplySettings() {
    AppConfig& cfg = ConfigManager::Get();

    int pigIdx = (int)SendMessageW(comboPigs_, CB_GETCURSEL, 0, 0);
    cfg.pigType = pigIdx + 1;

    int sizeIdx = (int)SendMessageW(comboSize_, CB_GETCURSEL, 0, 0);
    if (sizeIdx == 0) cfg.pigSize = 130;
    else if (sizeIdx == 2) cfg.pigSize = 240;
    else cfg.pigSize = 180;

    cfg.enableSound = (SendMessageW(chkSound_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    cfg.enableVoiceTTS = (SendMessageW(chkVoiceTTS_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    cfg.enablePigShadow = (SendMessageW(chkPigShadow_, BM_GETCHECK, 0, 0) == BST_CHECKED);

    bool oldIsland = cfg.enableDynamicIsland;
    cfg.enableDynamicIsland = (SendMessageW(chkDynamicIsland_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if (oldIsland != cfg.enableDynamicIsland) {
        OverlayWindow::Instance().SetDynamicIslandMode(cfg.enableDynamicIsland);
    }

    cfg.hideWhenNoDota = (SendMessageW(chkHide_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    cfg.autostart = (SendMessageW(chkAutostart_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    cfg.usePlayerName = (SendMessageW(chkUseName_, BM_GETCHECK, 0, 0) == BST_CHECKED);

    wchar_t nameBuf[128] = { 0 };
    GetWindowTextW(txtPlayerName_, nameBuf, 128);
    cfg.playerName = nameBuf;
    if (cfg.playerName.empty()) cfg.playerName = L"Марат";

    auto mics = VoiceAssistant::GetAvailableMicrophones();
    int mSel = (int)SendMessageW(comboMicrophone_, CB_GETCURSEL, 0, 0);
    std::wstring oldMic = cfg.microphoneDevice;
    if (mSel <= 0) {
        cfg.microphoneDevice = L"";
    } else if (mSel - 1 < (int)mics.size()) {
        cfg.microphoneDevice = mics[mSel - 1].first;
    }

    bool oldVoice = cfg.enableVoiceAssistant;
    cfg.enableVoiceAssistant = (SendMessageW(chkVoiceAssistant_, BM_GETCHECK, 0, 0) == BST_CHECKED);

    wchar_t wakeBuf[128] = { 0 };
    GetWindowTextW(txtWakeWord_, wakeBuf, 128);
    cfg.assistantWakeWord = wakeBuf;
    if (cfg.assistantWakeWord.empty()) cfg.assistantWakeWord = L"Свин";

    int gSel = (int)SendMessageW(comboVoiceGender_, CB_GETCURSEL, 0, 0);
    cfg.voiceGender = (gSel == 0) ? 0 : 1;
    cfg.enableToxicMode = (SendMessageW(chkToxicMode_, BM_GETCHECK, 0, 0) == BST_CHECKED);
    cfg.enableProactiveAdvisor = (SendMessageW(chkProactiveAdvisor_, BM_GETCHECK, 0, 0) == BST_CHECKED);

    VoiceService::Instance().ApplyVoiceSettings();

    if ((cfg.enableVoiceAssistant != oldVoice) || (oldMic != cfg.microphoneDevice && cfg.enableVoiceAssistant)) {
        VoiceAssistant::Instance().Stop();
        if (cfg.enableVoiceAssistant) {
            VoiceAssistant::Instance().Start();
        }
    }

    ConfigManager::Save();
    OverlayWindow::Instance().ReloadSkinAndSize();

    // Check visibility based on Dota state
    if (cfg.hideWhenNoDota && !DotaGsiService::Instance().IsDotaActive()) {
        OverlayWindow::Instance().Show(false);
    } else {
        OverlayWindow::Instance().Show(true);
    }
}

void SettingsWindow::UpdateDotaStatus(bool isRunning) {
    if (!lblStatus_) return;
    if (isRunning) {
        SetWindowTextW(lblStatus_, L"Статус: Dota 2 запущена!");
    } else {
        SetWindowTextW(lblStatus_, L"Статус: Dota 2 ожидает запуска...");
    }
}

void SettingsWindow::DrawPreview(HDC hdc, RECT rc) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    // Subtle background
    SolidBrush bg(Color(245, 247, 250));
    g.FillRectangle(&bg, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);

    Pen border(Color(210, 215, 220), 1.0f);
    g.DrawRectangle(&border, rc.left, rc.top, rc.right - rc.left - 1, rc.bottom - rc.top - 1);

    if (previewImage_ && previewImage_->GetLastStatus() == Ok) {
        int w = previewImage_->GetWidth();
        int h = previewImage_->GetHeight();

        float targetW = (float)(rc.right - rc.left - 20);
        float targetH = (float)(rc.bottom - rc.top - 20);

        float scale = (std::min)(targetW / w, targetH / h);
        float drawW = w * scale;
        float drawH = h * scale;
        float x = rc.left + (targetW + 20 - drawW) / 2.0f;
        float y = rc.top + (targetH + 20 - drawH) / 2.0f;

        g.DrawImage(previewImage_.get(), RectF(x, y, drawW, drawH));
    }
}

void SettingsWindow::Show(bool show) {
    visible_ = show;
    if (hwnd_) {
        if (show) {
            LoadCurrentSettingsToUI();
            ShowWindow(hwnd_, SW_SHOW);
            SetForegroundWindow(hwnd_);
        } else {
            ShowWindow(hwnd_, SW_HIDE);
        }
    }
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* self = (SettingsWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);

        if (id == IDC_COMBO_PIGS && code == CBN_SELCHANGE) {
            if (self) self->OnPigSelectionChanged();
        }
        else if (id == IDC_BTN_SAVE) {
            if (self) {
                self->ApplySettings();
                MessageBoxW(hwnd, L"Настройки успешно сохранены!", L"Bad Piggies Widget", MB_OK | MB_ICONINFORMATION);
            }
        }
        else if (id == IDC_BTN_TEST_KILL) {
            OverlayWindow::Instance().Show(true);
            OverlayWindow::Instance().TriggerKill(1, 0);
        }
        else if (id == IDC_BTN_TEST_DEATH) {
            OverlayWindow::Instance().Show(true);
            OverlayWindow::Instance().TriggerDeath();
        }
        else if (id == IDC_BTN_TEST_LOW_HP) {
            static bool testLowHp = false;
            testLowHp = !testLowHp;
            VignetteWindow::Instance().SetActive(testLowHp);
            OverlayWindow::Instance().Show(true);
            OverlayWindow::Instance().TriggerLowHp(testLowHp, 15);
        }
        else if (id == IDC_CHK_USE_NAME) {
            if (self) {
                bool chk = (SendMessageW(self->chkUseName_, BM_GETCHECK, 0, 0) == BST_CHECKED);
                EnableWindow(self->txtPlayerName_, chk ? TRUE : FALSE);
            }
        }
        else if (id == IDC_CHK_VOICE_ASSISTANT) {
            if (self) {
                bool chk = (SendMessageW(self->chkVoiceAssistant_, BM_GETCHECK, 0, 0) == BST_CHECKED);
                EnableWindow(self->txtWakeWord_, chk ? TRUE : FALSE);
            }
        }
        else if (id == IDC_BTN_TEST_ADVISOR) {
            VoiceAssistant::Instance().AskAdvice(0);
        }
        else if (id == IDC_BTN_CHECK_UPDATE) {
            Updater::Instance().CheckForUpdatesAsync(false, hwnd);
        }
        else if (id == IDC_BTN_SETUP_DOTA) {
            std::wstring msgResult;
            bool ok = DotaGsiService::Instance().SetupDotaConfig(msgResult);
            MessageBoxW(hwnd, msgResult.c_str(), ok ? L"Успех" : L"Ошибка", ok ? MB_OK | MB_ICONINFORMATION : MB_OK | MB_ICONERROR);
        }
        return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlID == IDC_PREVIEW && self) {
            self->DrawPreview(dis->hDC, dis->rcItem);
            return TRUE;
        }
        break;
    }
    case WM_CLOSE: {
        // Just hide to tray rather than exiting app
        if (self) self->Show(false);
        return 0;
    }
    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
