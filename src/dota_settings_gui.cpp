#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "dota_settings_gui.h"
#include "dota_welcome.h"
#include "config.h"
#include "overlay.h"
#include "vignette.h"
#include "dota_gsi.h"
#include "updater.h"
#include "voice_assistant.h"
#include "voice.h"
#include "hero_advisor.h"
#include <algorithm>

using namespace Gdiplus;

static const wchar_t* DOTA_SETTINGS_CLASS = L"BadPiggiesDotaSettingsOverlayClass";

DotaSettingsOverlay& DotaSettingsOverlay::Instance() {
    static DotaSettingsOverlay instance;
    return instance;
}

DotaSettingsOverlay::DotaSettingsOverlay() {}
DotaSettingsOverlay::~DotaSettingsOverlay() {
    Destroy();
}

bool DotaSettingsOverlay::Create() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = DotaSettingsOverlay::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = DOTA_SETTINGS_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width_) / 2;
    int y = (screenH - height_) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        DOTA_SETTINGS_CLASS,
        L"Bad Piggies Dota Settings",
        WS_POPUP,
        x, y, width_, height_,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    if (!hwnd_) return false;

    hFontEdit_ = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    editName_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"Марат",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        220, 377, 130, 24,
        hwnd_, NULL, GetModuleHandle(NULL), NULL
    );
    SendMessageW(editName_, WM_SETFONT, (WPARAM)hFontEdit_, TRUE);

    editWakeWord_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"Свин",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        430, 436, 120, 24,
        hwnd_, NULL, GetModuleHandle(NULL), NULL
    );
    SendMessageW(editWakeWord_, WM_SETFONT, (WPARAM)hFontEdit_, TRUE);

    return true;
}

void DotaSettingsOverlay::Destroy() {
    if (hFontEdit_) {
        DeleteObject(hFontEdit_);
        hFontEdit_ = NULL;
    }
    if (editWakeWord_) {
        DestroyWindow(editWakeWord_);
        editWakeWord_ = NULL;
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = NULL;
    }
}

void DotaSettingsOverlay::Show(bool show) {
    if (!hwnd_) Create();
    if (show) {
        cfgCopy_ = ConfigManager::Get();
        visible_ = true;
        if (editName_) {
            SetWindowTextW(editName_, cfgCopy_.playerName.c_str());
            EnableWindow(editName_, cfgCopy_.usePlayerName ? TRUE : FALSE);
        }
        if (editWakeWord_) {
            SetWindowTextW(editWakeWord_, cfgCopy_.assistantWakeWord.c_str());
            EnableWindow(editWakeWord_, cfgCopy_.enableVoiceAssistant ? TRUE : FALSE);
        }
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd_);
        InvalidateRect(hwnd_, NULL, TRUE);
    } else {
        visible_ = false;
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void DotaSettingsOverlay::Toggle() {
    Show(!visible_);
}

void DotaSettingsOverlay::SaveAndClose() {
    if (editName_) {
        wchar_t nBuf[128] = { 0 };
        GetWindowTextW(editName_, nBuf, 128);
        cfgCopy_.playerName = nBuf;
        if (cfgCopy_.playerName.empty()) cfgCopy_.playerName = L"Марат";
    }
    if (editWakeWord_) {
        wchar_t wBuf[128] = { 0 };
        GetWindowTextW(editWakeWord_, wBuf, 128);
        cfgCopy_.assistantWakeWord = wBuf;
        if (cfgCopy_.assistantWakeWord.empty()) cfgCopy_.assistantWakeWord = L"Свин";
    }

    bool oldVoice = ConfigManager::Get().enableVoiceAssistant;
    ConfigManager::Get() = cfgCopy_;
    ConfigManager::Save();

    VoiceService::Instance().ApplyVoiceSettings();

    if (cfgCopy_.enableVoiceAssistant != oldVoice) {
        if (cfgCopy_.enableVoiceAssistant) {
            VoiceAssistant::Instance().Start();
        } else {
            VoiceAssistant::Instance().Stop();
        }
    }

    OverlayWindow::Instance().ReloadSkinAndSize();
    Show(false);
}

static bool PtInButton(int px, int py, int bx, int by, int bw, int bh) {
    return (px >= bx && px <= bx + bw && py >= by && py <= by + bh);
}

void DotaSettingsOverlay::HandleClick(int x, int y) {
    // 1. Close button
    if (PtInButton(x, y, width_ - 45, 8, 35, 30)) {
        Show(false);
        return;
    }

    // 2. Pig skin buttons: Y=78, 25 + i * 88, 80x28
    for (int i = 0; i < 7; ++i) {
        int bx = 25 + i * 88;
        if (PtInButton(x, y, bx, 78, 80, 28)) {
            cfgCopy_.pigType = i + 1;
            InvalidateRect(hwnd_, NULL, FALSE);
            return;
        }
    }

    // 3. Pig size buttons: Y=115, 170 + i * 95, 90x24
    int sizes[] = { 120, 150, 180, 220, 260 };
    for (int i = 0; i < 5; ++i) {
        int bx = 170 + i * 95;
        if (PtInButton(x, y, bx, 115, 90, 24)) {
            cfgCopy_.pigSize = sizes[i];
            InvalidateRect(hwnd_, NULL, FALSE);
            return;
        }
    }

    // 4. Sound master toggle: (25, 175, 140, 28)
    if (PtInButton(x, y, 25, 175, 140, 28)) {
        cfgCopy_.enableSound = !cfgCopy_.enableSound;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }

    // 5. Volume buttons: [25%] [50%] [75%] [100%] at X=180 + i*65, Y=175, 55x28
    int vols[] = { 25, 50, 75, 100 };
    for (int i = 0; i < 4; ++i) {
        int bx = 180 + i * 65;
        if (PtInButton(x, y, bx, 175, 55, 28)) {
            cfgCopy_.soundVolume = vols[i];
            InvalidateRect(hwnd_, NULL, FALSE);
            return;
        }
    }

    // 6. Sound checkboxes:
    if (PtInButton(x, y, 25, 215, 280, 22)) {
        cfgCopy_.soundKill = !cfgCopy_.soundKill;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    if (PtInButton(x, y, 330, 215, 280, 22)) {
        cfgCopy_.soundDeath = !cfgCopy_.soundDeath;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    if (PtInButton(x, y, 25, 242, 280, 22)) {
        cfgCopy_.soundLowHp = !cfgCopy_.soundLowHp;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    if (PtInButton(x, y, 330, 242, 280, 22)) {
        cfgCopy_.soundLotus = !cfgCopy_.soundLotus;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }

    // 7. Reactions & Features checkboxes:
    // Vignette (25, 298, 600, 22)
    if (PtInButton(x, y, 25, 298, 600, 22)) {
        cfgCopy_.enableVignette = !cfgCopy_.enableVignette;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Beaten pig (25, 324, 600, 22)
    if (PtInButton(x, y, 25, 324, 600, 22)) {
        cfgCopy_.enableBeatenPig = !cfgCopy_.enableBeatenPig;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Lotus reminder checkbox (25, 350, 340, 22)
    if (PtInButton(x, y, 25, 350, 340, 22)) {
        cfgCopy_.enableLotusReminder = !cfgCopy_.enableLotusReminder;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Lotus mode button (380, 348, 260, 24): cycles Auto -> Normal 3m -> Turbo 1.5m
    if (PtInButton(x, y, 380, 348, 260, 24)) {
        cfgCopy_.lotusMode = (cfgCopy_.lotusMode + 1) % 3;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Use player name (25, 378, 190, 22)
    if (PtInButton(x, y, 25, 378, 190, 22)) {
        cfgCopy_.usePlayerName = !cfgCopy_.usePlayerName;
        if (editName_) EnableWindow(editName_, cfgCopy_.usePlayerName ? TRUE : FALSE);
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Voice TTS (25, 408, 300, 22)
    if (PtInButton(x, y, 25, 408, 300, 22)) {
        cfgCopy_.enableVoiceTTS = !cfgCopy_.enableVoiceTTS;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Voice Gender button (330, 405, 310, 24)
    if (PtInButton(x, y, 330, 405, 310, 24)) {
        cfgCopy_.voiceGender = (cfgCopy_.voiceGender == 1 ? 0 : 1);
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Voice Assistant (25, 436, 400, 22)
    if (PtInButton(x, y, 25, 436, 400, 22)) {
        cfgCopy_.enableVoiceAssistant = !cfgCopy_.enableVoiceAssistant;
        if (editWakeWord_) EnableWindow(editWakeWord_, cfgCopy_.enableVoiceAssistant ? TRUE : FALSE);
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Toxic mode (25, 464, 620, 22)
    if (PtInButton(x, y, 25, 464, 620, 22)) {
        cfgCopy_.enableToxicMode = !cfgCopy_.enableToxicMode;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Proactive advisor (25, 492, 620, 22)
    if (PtInButton(x, y, 25, 492, 620, 22)) {
        cfgCopy_.enableProactiveAdvisor = !cfgCopy_.enableProactiveAdvisor;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }
    // Hide when no dota (25, 520, 620, 22)
    if (PtInButton(x, y, 25, 520, 620, 22)) {
        cfgCopy_.hideWhenNoDota = !cfgCopy_.hideWhenNoDota;
        InvalidateRect(hwnd_, NULL, FALSE);
        return;
    }

    // 8. Test & Update buttons:
    // Kill: (25, 575, 95, 28)
    if (PtInButton(x, y, 25, 575, 95, 28)) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerKill(1, 0);
        return;
    }
    // Death: (128, 575, 95, 28)
    if (PtInButton(x, y, 128, 575, 95, 28)) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerDeath();
        return;
    }
    // Low HP: (231, 575, 95, 28)
    if (PtInButton(x, y, 231, 575, 95, 28)) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerLowHp(true, 15);
        return;
    }
    // Lotus: (334, 575, 95, 28)
    if (PtInButton(x, y, 334, 575, 95, 28)) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerLotus();
        return;
    }
    // Rampage test: (437, 575, 100, 28)
    if (PtInButton(x, y, 437, 575, 100, 28)) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerKill(5, 5);
        return;
    }
    // Check Updates: (545, 575, 110, 28)
    if (PtInButton(x, y, 545, 575, 110, 28)) {
        Updater::Instance().CheckForUpdatesAsync(false, hwnd_);
        return;
    }

    // Row 2 test buttons:
    // Buy Advice: (25, 610, 195, 28)
    if (PtInButton(x, y, 25, 610, 195, 28)) {
        VoiceAssistant::Instance().AskAdvice(0);
        return;
    }
    // Skill Advice: (230, 610, 195, 28)
    if (PtInButton(x, y, 230, 610, 195, 28)) {
        VoiceAssistant::Instance().AskAdvice(1);
        return;
    }
    // Bro Chat (F5): (435, 610, 220, 28)
    if (PtInButton(x, y, 435, 610, 220, 28)) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetFriendlyChat());
        return;
    }

    // 9. Save & Apply: (190, 680, 300, 44)
    if (PtInButton(x, y, 190, 680, 300, 44)) {
        SaveAndClose();
        return;
    }
}

void DotaSettingsOverlay::Render(HDC hdc) {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    Bitmap memBmp(w, h, PixelFormat32bppARGB);
    Graphics g(&memBmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Deep Dota dark background
    SolidBrush bgBrush(Color(255, 21, 24, 28));
    g.FillRectangle(&bgBrush, 0, 0, w, h);

    // Outer border
    Pen borderPen(Color(255, 60, 68, 80), 2.0f);
    g.DrawRectangle(&borderPen, 1, 1, w - 2, h - 2);

    // Header bar (Y: 0 to 45)
    SolidBrush headerBg(Color(255, 28, 33, 40));
    g.FillRectangle(&headerBg, 2, 2, w - 4, 42);

    // Red Dota accent stripe
    SolidBrush redStripe(Color(255, 194, 60, 42));
    g.FillRectangle(&redStripe, 2, 43, w - 4, 3);

    // Header title
    Gdiplus::Font headerFont(L"Segoe UI", 12.0f, FontStyleBold, UnitPoint);
    SolidBrush goldBrush(Color(255, 231, 210, 145));
    g.DrawString(L"DOTA 2 COMPANION — НАСТРОЙКИ WIDGET BAD PIGGIES", -1, &headerFont, PointF(20.0f, 12.0f), &goldBrush);

    // Close button [✕]
    SolidBrush closeBg(Color(255, 45, 52, 62));
    g.FillRectangle(&closeBg, w - 45, 8, 35, 30);
    SolidBrush closeText(Color(255, 220, 220, 220));
    g.DrawString(L"✕", -1, &headerFont, PointF((float)(w - 35), 12.0f), &closeText);

    Gdiplus::Font sectionFont(L"Segoe UI", 10.0f, FontStyleBold, UnitPoint);
    Gdiplus::Font normalFont(L"Segoe UI", 9.5f, FontStyleRegular, UnitPoint);
    SolidBrush whiteBrush(Color(255, 240, 240, 240));

    // --- Section 1: СВИНКА ---
    g.DrawString(L"ВЫБОР СВИНКИ:", -1, &sectionFont, PointF(25.0f, 54.0f), &goldBrush);
    for (int i = 0; i < 7; ++i) {
        int bx = 25 + i * 88;
        bool selected = (cfgCopy_.pigType == i + 1);
        SolidBrush btnBg(selected ? Color(255, 60, 140, 60) : Color(255, 35, 40, 48));
        g.FillRectangle(&btnBg, bx, 78, 80, 28);
        Pen bPen(selected ? Color(255, 100, 220, 100) : Color(255, 55, 62, 74), 1.5f);
        g.DrawRectangle(&bPen, bx, 78, 80, 28);

        std::wstring label = L"Вид " + std::to_wstring(i + 1);
        g.DrawString(label.c_str(), -1, &normalFont, PointF((float)bx + 16.0f, 82.0f), &whiteBrush);
    }

    g.DrawString(L"РАЗМЕР СВИНКИ:", -1, &sectionFont, PointF(25.0f, 117.0f), &goldBrush);
    int sizes[] = { 120, 150, 180, 220, 260 };
    for (int i = 0; i < 5; ++i) {
        int bx = 170 + i * 95;
        bool selected = (cfgCopy_.pigSize == sizes[i]);
        SolidBrush btnBg(selected ? Color(255, 180, 120, 30) : Color(255, 35, 40, 48));
        g.FillRectangle(&btnBg, bx, 115, 90, 24);
        Pen bPen(selected ? Color(255, 240, 170, 50) : Color(255, 55, 62, 74), 1.2f);
        g.DrawRectangle(&bPen, bx, 115, 90, 24);

        std::wstring sLabel = std::to_wstring(sizes[i]) + L" px";
        g.DrawString(sLabel.c_str(), -1, &normalFont, PointF((float)bx + 20.0f, 118.0f), &whiteBrush);
    }

    // --- Section 2: ЗВУКИ И ГРОМКОСТЬ ---
    g.DrawString(L"ЗВУКОВЫЕ ЭФФЕКТЫ:", -1, &sectionFont, PointF(25.0f, 150.0f), &goldBrush);
    {
        bool snd = cfgCopy_.enableSound;
        SolidBrush sndBg(snd ? Color(255, 40, 110, 40) : Color(255, 110, 40, 40));
        g.FillRectangle(&sndBg, 25, 175, 140, 28);
        Pen sndPen(snd ? Color(255, 70, 180, 70) : Color(255, 180, 60, 60), 1.5f);
        g.DrawRectangle(&sndPen, 25, 175, 140, 28);
        std::wstring sndText = snd ? L"ЗВУК: ВКЛЮЧЕН" : L"ЗВУК: ВЫКЛЮЧЕН";
        g.DrawString(sndText.c_str(), -1, &normalFont, PointF(34.0f, 180.0f), &whiteBrush);
    }
    int vols[] = { 25, 50, 75, 100 };
    for (int i = 0; i < 4; ++i) {
        int bx = 180 + i * 65;
        bool selected = (cfgCopy_.soundVolume == vols[i]);
        SolidBrush btnBg(selected ? Color(255, 60, 90, 140) : Color(255, 35, 40, 48));
        g.FillRectangle(&btnBg, bx, 175, 55, 28);
        Pen bPen(selected ? Color(255, 100, 150, 230) : Color(255, 55, 62, 74), 1.2f);
        g.DrawRectangle(&bPen, bx, 175, 55, 28);
        std::wstring vLabel = std::to_wstring(vols[i]) + L"%";
        g.DrawString(vLabel.c_str(), -1, &normalFont, PointF((float)bx + 12.0f, 180.0f), &whiteBrush);
    }

    auto drawCheckbox = [&](int x, int y, bool val, const wchar_t* text) {
        SolidBrush boxBg(Color(255, 30, 35, 42));
        g.FillRectangle(&boxBg, x, y + 2, 18, 18);
        Pen boxPen(val ? Color(255, 231, 210, 145) : Color(255, 70, 78, 90), 1.5f);
        g.DrawRectangle(&boxPen, x, y + 2, 18, 18);
        if (val) {
            SolidBrush checkMark(Color(255, 231, 210, 145));
            g.FillRectangle(&checkMark, x + 4, y + 6, 10, 10);
        }
        g.DrawString(text, -1, &normalFont, PointF((float)x + 26.0f, (float)y + 2.0f), &whiteBrush);
    };

    drawCheckbox(25, 215, cfgCopy_.soundKill, L"Звук при убийстве (Килл)");
    drawCheckbox(330, 215, cfgCopy_.soundDeath, L"Звук при смерти героя");
    drawCheckbox(25, 242, cfgCopy_.soundLowHp, L"Звук тревоги при Low HP");
    drawCheckbox(330, 242, cfgCopy_.soundLotus, L"Звук появления Лотуса (Колокольчик)");

    // --- Section 3: РЕАКЦИИ И ИНТЕГРАЦИЯ ---
    g.DrawString(L"РЕАКЦИИ И ИНТЕГРАЦИЯ:", -1, &sectionFont, PointF(25.0f, 275.0f), &goldBrush);
    drawCheckbox(25, 298, cfgCopy_.enableVignette, L"Красная пульсирующая виньетка по краям экрана при Low HP (<= 25%)");
    drawCheckbox(25, 324, cfgCopy_.enableBeatenPig, L"Превращаться в побитого свина при Low HP (шлем и синяк)");
    drawCheckbox(25, 350, cfgCopy_.enableLotusReminder, L"Напоминание о созревании Лотусов:");
    {
        const wchar_t* lModeLabels[] = {
            L"Режим: Авто (3м / 1.5м Турбо)",
            L"Режим: Обычный (3 мин)",
            L"Режим: Турбо (1.5 мин)"
        };
        int mIdx = cfgCopy_.lotusMode;
        if (mIdx < 0 || mIdx > 2) mIdx = 0;
        SolidBrush lmBg(mIdx == 2 ? Color(255, 120, 50, 30) : (mIdx == 1 ? Color(255, 35, 75, 110) : Color(255, 40, 80, 50)));
        g.FillRectangle(&lmBg, 350, 348, 290, 24);
        Pen lmPen(Color(255, 100, 160, 100), 1.2f);
        g.DrawRectangle(&lmPen, 350, 348, 290, 24);
        g.DrawString(lModeLabels[mIdx], -1, &normalFont, PointF(358.0f, 351.0f), &whiteBrush);
    }

    // Name checkbox and note
    drawCheckbox(25, 378, cfgCopy_.usePlayerName, L"Обращаться к вам:");
    g.DrawString(L"(Имя только в «Красава, Марат!»)", -1, &normalFont, PointF(360.0f, 380.0f), &goldBrush);

    drawCheckbox(25, 408, cfgCopy_.enableVoiceTTS, L"Голосовая озвучка реплик (TTS):");
    {
        const wchar_t* vLabels[] = {
            L"Голос: Женский (Ирина)",
            L"Голос: Мужской (Павел)"
        };
        int vIdx = (cfgCopy_.voiceGender == 0 ? 0 : 1);
        SolidBrush vBg(Color(255, 35, 75, 110));
        g.FillRectangle(&vBg, 330, 405, 310, 24);
        Pen vPen(Color(255, 100, 160, 220), 1.2f);
        g.DrawRectangle(&vPen, 330, 405, 310, 24);
        g.DrawString(vLabels[vIdx], -1, &normalFont, PointF(338.0f, 408.0f), &whiteBrush);
    }

    // Voice assistant & wake word
    drawCheckbox(25, 436, cfgCopy_.enableVoiceAssistant, L"Голосовой помощник (микрофон: «Свин, что купить?» / «Пиг»):");
    g.DrawString(L"(или Пиг)", -1, &normalFont, PointF(558.0f, 438.0f), &goldBrush);

    drawCheckbox(25, 464, cfgCopy_.enableToxicMode, L"🔥 Говорить наравне с человеком (мощная прожарка / токсик)");
    drawCheckbox(25, 492, cfgCopy_.enableProactiveAdvisor, L"Умный мета-советник Dota2ProTracker (тайминги, инвентарь, скиллы)");
    drawCheckbox(25, 520, cfgCopy_.hideWhenNoDota, L"Автоматически скрывать свина, когда игра Dota 2 не запущена");

    // --- Section 4: БЫСТРЫЙ ТЕСТ ---
    g.DrawString(L"БЫСТРЫЙ ТЕСТ И ОБНОВЛЕНИЯ:", -1, &sectionFont, PointF(25.0f, 552.0f), &goldBrush);
    auto drawTestBtn = [&](int bx, int by, int bw, int bh, const wchar_t* bText) {
        SolidBrush tBg(Color(255, 36, 42, 50));
        g.FillRectangle(&tBg, bx, by, bw, bh);
        Pen tPen(Color(255, 65, 75, 90), 1.2f);
        g.DrawRectangle(&tPen, bx, by, bw, bh);
        g.DrawString(bText, -1, &normalFont, PointF((float)bx + 8.0f, (float)by + 5.0f), &whiteBrush);
    };

    drawTestBtn(25, 575, 95, 28, L"Тест: Килл");
    drawTestBtn(128, 575, 95, 28, L"Тест: Смерть");
    drawTestBtn(231, 575, 95, 28, L"Тест: Low HP");
    drawTestBtn(334, 575, 95, 28, L"Тест: Лотус");
    drawTestBtn(437, 575, 100, 28, L"🔥 РАМПАГА!");
    drawTestBtn(545, 575, 110, 28, L"🔄 Обновить");

    drawTestBtn(25, 610, 195, 28, L"🧠 F6: Что купить?");
    drawTestBtn(230, 610, 195, 28, L"⚡ F7: Что качать?");
    drawTestBtn(435, 610, 220, 28, L"💬 F5: Поболтать!");

    // --- Footer ---
    g.DrawString(L"Подсказка: зажмите Left Ctrl и перетаскивайте свина левой кнопкой мыши в любое место!", -1, &normalFont, PointF(35.0f, 650.0f), &goldBrush);

    // Save & Close Action Button
    SolidBrush saveBg(Color(255, 160, 45, 35));
    g.FillRectangle(&saveBg, 190, 680, 300, 44);
    Pen savePen(Color(255, 230, 75, 60), 2.0f);
    g.DrawRectangle(&savePen, 190, 680, 300, 44);

    Gdiplus::Font saveFont(L"Segoe UI", 12.0f, FontStyleBold, UnitPoint);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF saveRect(190.0f, 680.0f, 300.0f, 44.0f);
    g.DrawString(L"СОХРАНИТЬ И ПРИМЕНИТЬ", -1, &saveFont, saveRect, &sf, &whiteBrush);

    // Blit to screen HDC
    Graphics screenG(hdc);
    screenG.DrawImage(&memBmp, 0, 0);
}

LRESULT CALLBACK DotaSettingsOverlay::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DotaSettingsOverlay* pThis = (DotaSettingsOverlay*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        pThis = (DotaSettingsOverlay*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    if (!pThis) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        pThis->Render(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, RGB(35, 40, 48));
        static HBRUSH hbrEdit = CreateSolidBrush(RGB(35, 40, 48));
        return (LRESULT)hbrEdit;
    }
    case WM_LBUTTONDOWN: {
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        pThis->HandleClick(x, y);
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            pThis->Show(false);
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
