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
#include "vosk_engine.h"
#include "voice_assistant.h"
#include "voice.h"
#include "hero_advisor.h"
#include "ai_brain.h"
#include <algorithm>
#include <commctrl.h>
#include <uxtheme.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

using namespace Gdiplus;

static const wchar_t* SETTINGS_CLASS = L"BadPiggiesSettingsModernClass";

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
    IDC_BTN_TEST_ADVISOR = 1020,
    IDC_BTN_TEST_MEME = 1021,
    IDC_BTN_TEST_PLAYLIST = 1022
};

SettingsWindow& SettingsWindow::Instance() {
    static SettingsWindow instance;
    return instance;
}

SettingsWindow::SettingsWindow() {}

SettingsWindow::~SettingsWindow() {
    if (hFontTitle_) DeleteObject(hFontTitle_);
    if (hFont_) DeleteObject(hFont_);
    if (hFontBold_) DeleteObject(hFontBold_);
    if (hFontSmall_) DeleteObject(hFontSmall_);

    if (hBrBkg_) DeleteObject(hBrBkg_);
    if (hBrCard_) DeleteObject(hBrCard_);
    if (hBrEdit_) DeleteObject(hBrEdit_);

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

    hBrBkg_ = CreateSolidBrush(RGB(14, 19, 26));
    hBrCard_ = CreateSolidBrush(RGB(22, 29, 39));
    hBrEdit_ = CreateSolidBrush(RGB(26, 35, 47));

    wc.hbrBackground = hBrBkg_;

    HICON hAppIcon = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!hAppIcon) {
        hAppIcon = (HICON)LoadImageW(NULL, ConfigManager::GetAssetPath(L"app.ico").c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
    }
    if (!hAppIcon) hAppIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIcon = hAppIcon;
    RegisterClassExW(&wc);

    hFontTitle_ = CreateFontW(19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    hFontBold_ = CreateFontW(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    hFont_ = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    hFontSmall_ = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    int winW = 630;
    int winH = 780;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        SETTINGS_CLASS,
        L"Bad Piggies Widget — Cyber Emerald & Vosk Edition (2026)",
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

    // =========================================================================
    // CARD 1: PIG SELECTION & LIVE PREVIEW (Y = 80 to 260)
    // =========================================================================
    HWND lblPig = CreateWindowW(L"STATIC", L"Внешний вид свинки:",
        WS_CHILD | WS_VISIBLE,
        35, 108, 280, 18, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblPig, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    comboPigs_ = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        35, 130, 290, 260, hwnd_, (HMENU)IDC_COMBO_PIGS, hInst, NULL);
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
        35, 172, 280, 18, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblSize, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    comboSize_ = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        35, 194, 290, 150, hwnd_, (HMENU)IDC_COMBO_SIZE, hInst, NULL);
    SendMessageW(comboSize_, WM_SETFONT, (WPARAM)hFont_, TRUE);
    SendMessageW(comboSize_, CB_ADDSTRING, 0, (LPARAM)L"Маленький (130 px)");
    SendMessageW(comboSize_, CB_ADDSTRING, 0, (LPARAM)L"Средний (180 px - Рекомендуется)");
    SendMessageW(comboSize_, CB_ADDSTRING, 0, (LPARAM)L"Большой (240 px)");

    previewBox_ = CreateWindowW(L"STATIC", NULL,
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        365, 96, 230, 150, hwnd_, (HMENU)IDC_PREVIEW, hInst, NULL);

    // =========================================================================
    // CARD 2: VOSK VOICE ASSISTANT & TIKTOK MEMES (Y = 270 to 625, Left Column)
    // =========================================================================
    chkVoiceAssistant_ = CreateWindowW(L"BUTTON", L"Голосовой помощник (Vosk Офлайн AI)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 295, 290, 22, hwnd_, (HMENU)IDC_CHK_VOICE_ASSISTANT, hInst, NULL);
    SendMessageW(chkVoiceAssistant_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    HWND lblMic = CreateWindowW(L"STATIC", L"Микрофон:",
        WS_CHILD | WS_VISIBLE,
        35, 324, 75, 18, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblMic, WM_SETFONT, (WPARAM)hFont_, TRUE);

    comboMicrophone_ = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        115, 322, 210, 160, hwnd_, NULL, hInst, NULL);
    SendMessageW(comboMicrophone_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    HWND lblWake = CreateWindowW(L"STATIC", L"Имя (wake word):",
        WS_CHILD | WS_VISIBLE,
        35, 355, 120, 18, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblWake, WM_SETFONT, (WPARAM)hFont_, TRUE);

    txtWakeWord_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Свин",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        160, 352, 165, 22, hwnd_, (HMENU)IDC_TXT_WAKE_WORD, hInst, NULL);
    SendMessageW(txtWakeWord_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkToxicMode_ = CreateWindowW(L"BUTTON", L"Режим жесткой прожарки (Токсик / Рофлы)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 384, 290, 20, hwnd_, NULL, hInst, NULL);
    SendMessageW(chkToxicMode_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    chkProactiveAdvisor_ = CreateWindowW(L"BUTTON", L"Советы по игре (закуп, скиллы, тайминги)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 410, 290, 20, hwnd_, (HMENU)IDC_CHK_PROACTIVE_ADVISOR, hInst, NULL);
    SendMessageW(chkProactiveAdvisor_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    chkUseName_ = CreateWindowW(L"BUTTON", L"Обращаться к вам по имени:",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 438, 175, 20, hwnd_, (HMENU)IDC_CHK_USE_NAME, hInst, NULL);
    SendMessageW(chkUseName_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    txtPlayerName_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Марат",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        215, 436, 110, 22, hwnd_, (HMENU)IDC_TXT_PLAYER_NAME, hInst, NULL);
    SendMessageW(txtPlayerName_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkSound_ = CreateWindowW(L"BUTTON", L"Звуковые эффекты (WAV киллы, лотусы)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 468, 290, 20, hwnd_, (HMENU)IDC_CHK_SOUND, hInst, NULL);
    SendMessageW(chkSound_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkVoiceTTS_ = CreateWindowW(L"BUTTON", L"Голосовая озвучка (TTS):",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 494, 160, 20, hwnd_, (HMENU)IDC_CHK_VOICE_TTS, hInst, NULL);
    SendMessageW(chkVoiceTTS_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    comboVoiceGender_ = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        200, 492, 125, 120, hwnd_, NULL, hInst, NULL);
    SendMessageW(comboVoiceGender_, WM_SETFONT, (WPARAM)hFont_, TRUE);
    SendMessageW(comboVoiceGender_, CB_ADDSTRING, 0, (LPARAM)L"Ирина (Ж)");
    SendMessageW(comboVoiceGender_, CB_ADDSTRING, 0, (LPARAM)L"Павел (М)");

    chkPigShadow_ = CreateWindowW(L"BUTTON", L"Объёмная 3D-тень свина",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 524, 290, 20, hwnd_, NULL, hInst, NULL);
    SendMessageW(chkPigShadow_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkDynamicIsland_ = CreateWindowW(L"BUTTON", L"Режим Dynamic Island (вверху экрана)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 550, 290, 20, hwnd_, NULL, hInst, NULL);
    SendMessageW(chkDynamicIsland_, WM_SETFONT, (WPARAM)hFontBold_, TRUE);

    chkHide_ = CreateWindowW(L"BUTTON", L"Скрывать виджет, если Дота закрыта",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 576, 290, 20, hwnd_, (HMENU)IDC_CHK_HIDE, hInst, NULL);
    SendMessageW(chkHide_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    chkAutostart_ = CreateWindowW(L"BUTTON", L"Автозапуск вместе с Windows",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 602, 290, 20, hwnd_, (HMENU)IDC_CHK_AUTOSTART, hInst, NULL);
    SendMessageW(chkAutostart_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    // =========================================================================
    // CARD 3: INTERACTIVE TESTING & TIKTOK MEMES (Y = 270 to 625, Right Column)
    // =========================================================================
    btnTestKill_ = CreateWindowW(L"BUTTON", L"🎯 Тест: Килл! (Прыжок)",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        365, 292, 230, 28, hwnd_, (HMENU)IDC_BTN_TEST_KILL, hInst, NULL);

    btnTestDeath_ = CreateWindowW(L"BUTTON", L"💀 Тест: Смерть (Роаст)",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        365, 324, 230, 28, hwnd_, (HMENU)IDC_BTN_TEST_DEATH, hInst, NULL);

    btnTestLowHp_ = CreateWindowW(L"BUTTON", L"🩸 Тест: Low HP (Виньетка)",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        365, 356, 230, 28, hwnd_, (HMENU)IDC_BTN_TEST_LOW_HP, hInst, NULL);

    btnTestAdvisor_ = CreateWindowW(L"BUTTON", L"🧠 Тест: Свин, что купить?",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        365, 388, 230, 28, hwnd_, (HMENU)IDC_BTN_TEST_ADVISOR, hInst, NULL);

    btnTestMeme_ = CreateWindowW(L"BUTTON", L"🔥 Тест: Мем из Тиктока!",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        365, 420, 230, 28, hwnd_, (HMENU)IDC_BTN_TEST_MEME, hInst, NULL);

    btnTestPlaylist_ = CreateWindowW(L"BUTTON", L"🎵 Тест: Включи мой плейлист",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        365, 452, 230, 30, hwnd_, (HMENU)IDC_BTN_TEST_PLAYLIST, hInst, NULL);

    btnSetupDota_ = CreateWindowW(L"BUTTON", L"🎮 Настроить CFG Dota 2 GSI",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        365, 486, 230, 28, hwnd_, (HMENU)IDC_BTN_SETUP_DOTA, hInst, NULL);

    btnCheckUpdate_ = CreateWindowW(L"BUTTON", L"🔄 Проверить обновления",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        365, 518, 230, 26, hwnd_, (HMENU)IDC_BTN_CHECK_UPDATE, hInst, NULL);

    std::string vStr = ConfigManager::Get().appVersion;
    std::wstring verStr = L"Версия: v" + std::wstring(vStr.begin(), vStr.end()) + L" • Vosk AI Edition 2026";
    HWND lblVer = CreateWindowW(L"STATIC", verStr.c_str(),
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        365, 580, 230, 16, hwnd_, NULL, hInst, NULL);
    SendMessageW(lblVer, WM_SETFONT, (WPARAM)hFontSmall_, TRUE);

    // Bottom Status Bar
    lblStatus_ = CreateWindowW(L"STATIC", L"Статус: Поиск Dota 2...",
        WS_CHILD | WS_VISIBLE,
        25, 646, 580, 20, hwnd_, (HMENU)IDC_LBL_STATUS, hInst, NULL);
    SendMessageW(lblStatus_, WM_SETFONT, (WPARAM)hFont_, TRUE);

    // Save Button
    btnSave_ = CreateWindowW(L"BUTTON", L"💾 СОХРАНИТЬ И ПРИМЕНИТЬ",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        175, 678, 280, 48, hwnd_, (HMENU)IDC_BTN_SAVE, hInst, NULL);
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
    auto mics = VoiceAssistant::GetAvailableMicrophones();
    int selMicIdx = 0;
    for (size_t i = 0; i < mics.size(); ++i) {
        SendMessageW(comboMicrophone_, CB_ADDSTRING, 0, (LPARAM)mics[i].first.c_str());
        if (!cfg.microphoneDevice.empty() && (cfg.microphoneDevice == mics[i].first || cfg.microphoneDevice == mics[i].second)) {
            selMicIdx = (int)i;
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
    } else if (mSel < (int)mics.size()) {
        cfg.microphoneDevice = mics[mSel].first;
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

    if (cfg.hideWhenNoDota && !DotaGsiService::Instance().IsDotaActive()) {
        OverlayWindow::Instance().Show(false);
    } else {
        OverlayWindow::Instance().Show(true);
    }
}

void SettingsWindow::UpdateDotaStatus(bool isRunning) {
    if (!lblStatus_) return;
    std::wstring voskStatus = VoskEngine::Instance().IsModelLoaded() ? L" • 🟢 Vosk Офлайн AI" : L" • ⚪ SAPI";
    if (isRunning) {
        SetWindowTextW(lblStatus_, (L"Статус: Dota 2 запущена!" + voskStatus).c_str());
    } else {
        SetWindowTextW(lblStatus_, (L"Статус: Dota 2 ожидает запуска..." + voskStatus).c_str());
    }
}

void SettingsWindow::SetLastRecognizedText(const std::wstring& text) {
    if (!lblStatus_) return;
    std::wstring display = L"🎤 Услышал: \"" + text + L"\"";
    SetWindowTextW(lblStatus_, display.c_str());
}

void SettingsWindow::SetLastAnswerText(const std::wstring& answer) {
    if (!lblStatus_) return;
    std::wstring display = L"💬 Свин ответил: \"" + answer + L"\"";
    SetWindowTextW(lblStatus_, display.c_str());
}

static void AddRoundedRect(GraphicsPath& path, RectF rect, float radius) {
    float diameter = radius * 2.0f;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter, diameter, diameter, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
}

void SettingsWindow::DrawPreview(HDC hdc, RECT rc) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    float w = (float)(rc.right - rc.left);
    float h = (float)(rc.bottom - rc.top);

    // Modern dark pedestal background
    GraphicsPath cardPath;
    AddRoundedRect(cardPath, RectF(0, 0, w, h), 12.0f);
    SolidBrush bg(Color(255, 18, 24, 33));
    g.FillPath(&bg, &cardPath);

    // Glowing emerald pedestal ring
    Pen ringPen(Color(120, 16, 185, 129), 1.5f);
    g.DrawEllipse(&ringPen, w * 0.15f, h * 0.72f, w * 0.70f, h * 0.22f);

    // Border
    Pen border(Color(180, 38, 51, 68), 1.2f);
    g.DrawPath(&border, &cardPath);

    if (previewImage_ && previewImage_->GetLastStatus() == Ok) {
        int imgW = previewImage_->GetWidth();
        int imgH = previewImage_->GetHeight();

        float targetW = w - 30.0f;
        float targetH = h - 30.0f;

        float scale = (std::min)(targetW / imgW, targetH / imgH);
        float drawW = imgW * scale;
        float drawH = imgH * scale;
        float x = (w - drawW) / 2.0f;
        float y = (h - drawH) / 2.0f - 4.0f;

        g.DrawImage(previewImage_.get(), RectF(x, y, drawW, drawH));
    }
}

void SettingsWindow::DrawOwnerButton(DRAWITEMSTRUCT* dis) {
    Graphics g(dis->hDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    RECT r = dis->rcItem;
    float w = (float)(r.right - r.left);
    float h = (float)(r.bottom - r.top);
    bool pressed = (dis->itemState & ODS_SELECTED);

    GraphicsPath btnPath;
    AddRoundedRect(btnPath, RectF(0, 0, w, h), 7.0f);

    Color topColor, botColor, borderColor, textColor;
    int ctlId = dis->CtlID;

    if (ctlId == IDC_BTN_SAVE) {
        // Emerald Gradient Action Button
        topColor = pressed ? Color(255, 12, 140, 95) : Color(255, 16, 185, 129);
        botColor = pressed ? Color(255, 8, 110, 75)  : Color(255, 5, 150, 105);
        borderColor = Color(255, 52, 211, 153);
        textColor = Color(255, 255, 255, 255);
    } else if (ctlId == IDC_BTN_TEST_KILL) {
        topColor = pressed ? Color(255, 20, 35, 30) : Color(255, 26, 46, 38);
        botColor = pressed ? Color(255, 14, 25, 20) : Color(255, 18, 33, 26);
        borderColor = Color(255, 16, 185, 129);
        textColor = Color(255, 52, 211, 153);
    } else if (ctlId == IDC_BTN_TEST_DEATH) {
        topColor = pressed ? Color(255, 45, 20, 25) : Color(255, 55, 25, 30);
        botColor = pressed ? Color(255, 30, 12, 16) : Color(255, 36, 16, 20);
        borderColor = Color(255, 239, 68, 68);
        textColor = Color(255, 252, 165, 165);
    } else if (ctlId == IDC_BTN_TEST_LOW_HP) {
        topColor = pressed ? Color(255, 45, 18, 30) : Color(255, 56, 22, 36);
        botColor = pressed ? Color(255, 30, 10, 20) : Color(255, 38, 14, 24);
        borderColor = Color(255, 244, 63, 94);
        textColor = Color(255, 253, 164, 175);
    } else if (ctlId == IDC_BTN_TEST_MEME) {
        // TikTok / Cyberpunk Neon Purple
        topColor = pressed ? Color(255, 45, 20, 50) : Color(255, 58, 25, 65);
        botColor = pressed ? Color(255, 30, 12, 35) : Color(255, 40, 15, 45);
        borderColor = Color(255, 217, 70, 239);
        textColor = Color(255, 245, 208, 254);
    } else if (ctlId == IDC_BTN_TEST_ADVISOR) {
        topColor = pressed ? Color(255, 18, 35, 48) : Color(255, 24, 46, 62);
        botColor = pressed ? Color(255, 12, 24, 34) : Color(255, 16, 32, 44);
        borderColor = Color(255, 6, 182, 212);
        textColor = Color(255, 165, 243, 252);
    } else if (ctlId == IDC_BTN_TEST_PLAYLIST) {
        topColor = pressed ? Color(255, 45, 35, 15) : Color(255, 58, 44, 18);
        botColor = pressed ? Color(255, 30, 22, 10) : Color(255, 40, 28, 12);
        borderColor = Color(255, 245, 158, 11); // Amber / Gold neon
        textColor = Color(255, 254, 243, 199);
    } else {
        topColor = pressed ? Color(255, 22, 30, 40) : Color(255, 30, 41, 55);
        botColor = pressed ? Color(255, 15, 20, 28) : Color(255, 20, 28, 38);
        borderColor = Color(255, 71, 85, 105);
        textColor = Color(255, 226, 232, 240);
    }

    LinearGradientBrush br(RectF(0, 0, w, h), topColor, botColor, LinearGradientModeVertical);
    g.FillPath(&br, &btnPath);

    Pen borderPen(borderColor, 1.2f);
    g.DrawPath(&borderPen, &btnPath);

    wchar_t btnText[128] = { 0 };
    GetWindowTextW(dis->hwndItem, btnText, 128);

    Gdiplus::Font font(L"Segoe UI", (ctlId == IDC_BTN_SAVE) ? 10.5f : 9.0f, (ctlId == IDC_BTN_SAVE) ? FontStyleBold : FontStyleBold, UnitPoint);
    SolidBrush textBr(textColor);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(btnText, -1, &font, RectF(0, 0, w, h), &sf, &textBr);
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
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        RECT clientRc;
        GetClientRect(hwnd, &clientRc);
        float cw = (float)(clientRc.right - clientRc.left);

        // 1. Header Banner (0 to 72px)
        LinearGradientBrush headerBr(RectF(0, 0, cw, 72.0f), Color(255, 18, 25, 35), Color(255, 12, 16, 23), LinearGradientModeVertical);
        g.FillRectangle(&headerBr, 0.0f, 0.0f, cw, 72.0f);

        // Neon Accent Stripe
        Pen stripePen(Color(255, 16, 185, 129), 2.0f);
        g.DrawLine(&stripePen, 0.0f, 72.0f, cw, 72.0f);

        // Header Title
        Gdiplus::Font titleFont(L"Segoe UI", 13.0f, FontStyleBold, UnitPoint);
        SolidBrush titleBrush(Color(255, 248, 250, 252));
        g.DrawString(L"🐷 BAD PIGGIES — AI COMPANION 2026", -1, &titleFont, PointF(22.0f, 14.0f), &titleBrush);

        // Subtitle with badges
        Gdiplus::Font subFont(L"Segoe UI", 9.0f, FontStyleRegular, UnitPoint);
        SolidBrush subBrush(Color(255, 16, 185, 129));
        g.DrawString(L"🎤 Vosk Offline Voice Engine  •  🔥 TikTok Meme Edition  •  🎮 Dota 2 GSI", -1, &subFont, PointF(24.0f, 42.0f), &subBrush);

        // 2. Card Backgrounds
        // Card 1: Pig & Preview (20, 84, 590, 175)
        GraphicsPath c1;
        AddRoundedRect(c1, RectF(20.0f, 84.0f, cw - 40.0f, 175.0f), 10.0f);
        SolidBrush cardBr(Color(255, 20, 27, 37));
        g.FillPath(&cardBr, &c1);
        Pen borderPen(Color(180, 35, 47, 63), 1.0f);
        g.DrawPath(&borderPen, &c1);

        // Card 2: Voice & Memes (20, 270, 318, 365)
        GraphicsPath c2;
        AddRoundedRect(c2, RectF(20.0f, 270.0f, 320.0f, 365.0f), 10.0f);
        g.FillPath(&cardBr, &c2);
        g.DrawPath(&borderPen, &c2);

        // Card 3: Interactive Testing (350, 270, 260, 365)
        GraphicsPath c3;
        AddRoundedRect(c3, RectF(350.0f, 270.0f, cw - 370.0f, 365.0f), 10.0f);
        g.FillPath(&cardBr, &c3);
        g.DrawPath(&borderPen, &c3);

        // Section Headers
        Gdiplus::Font secFont(L"Segoe UI", 8.5f, FontStyleBold, UnitPoint);
        SolidBrush secBrush(Color(255, 148, 163, 184));
        g.DrawString(L"👑 ВЫБОР СВИНКИ И ПРЕВЬЮ", -1, &secFont, PointF(32.0f, 90.0f), &secBrush);
        g.DrawString(L"🎤 VOSK ОФЛАЙН ПОМОЩНИК И ОПЦИИ", -1, &secFont, PointF(32.0f, 276.0f), &secBrush);
        g.DrawString(L"⚡ БЫСТРЫЙ ТЕСТ И МЕМЫ", -1, &secFont, PointF(362.0f, 276.0f), &secBrush);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(226, 232, 240));
        SetBkMode(hdcStatic, TRANSPARENT);
        if (self && self->hBrCard_) return (LRESULT)self->hBrCard_;
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_CTLCOLOREDIT: {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, RGB(248, 250, 252));
        SetBkColor(hdcEdit, RGB(26, 35, 47));
        if (self && self->hBrEdit_) return (LRESULT)self->hBrEdit_;
        return (LRESULT)GetStockObject(BLACK_BRUSH);
    }
    case WM_CTLCOLORBTN: {
        HDC hdcBtn = (HDC)wParam;
        SetTextColor(hdcBtn, RGB(226, 232, 240));
        SetBkMode(hdcBtn, TRANSPARENT);
        if (self && self->hBrCard_) return (LRESULT)self->hBrCard_;
        return (LRESULT)GetStockObject(NULL_BRUSH);
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
                MessageBoxW(hwnd, L"Настройки успешно сохранены и применены!", L"Bad Piggies Widget", MB_OK | MB_ICONINFORMATION);
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
        else if (id == IDC_BTN_TEST_ADVISOR) {
            VoiceAssistant::Instance().AskAdvice(0);
        }
        else if (id == IDC_BTN_TEST_MEME) {
            VoiceAssistant::Instance().TriggerMeme();
        }
        else if (id == IDC_BTN_TEST_PLAYLIST) {
            AiBrain::Instance().OpenMusicPlaylist();
            std::wstring serviceRu = AiMemory::Instance().GetMusicServiceNameRu();
            std::wstring reply = L"Включаю твой плейлист в " + serviceRu + L"! Свин всё помнит и готов тащить.";
            OverlayWindow::Instance().TriggerAdvice(reply);
            SettingsWindow::Instance().SetLastAnswerText(reply);
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
        if (self && (dis->CtlType == ODT_BUTTON)) {
            self->DrawOwnerButton(dis);
            return TRUE;
        }
        break;
    }
    case WM_SHOWWINDOW: {
        if (wParam && self) {
            self->LoadCurrentSettingsToUI();
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;
    }
    case WM_CLOSE: {
        if (self) self->Show(false);
        return 0;
    }
    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
