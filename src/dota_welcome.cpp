#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "dota_welcome.h"
#include "config.h"
#include "overlay.h"
#include "hero_advisor.h"
#include <algorithm>
#include <mmsystem.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

using namespace Gdiplus;

static const wchar_t* DOTA_WELCOME_CLASS = L"BadPiggiesDotaWelcomeClass";

DotaWelcomeOverlay& DotaWelcomeOverlay::Instance() {
    static DotaWelcomeOverlay instance;
    return instance;
}

DotaWelcomeOverlay::DotaWelcomeOverlay() {}
DotaWelcomeOverlay::~DotaWelcomeOverlay() {
    Destroy();
}

bool DotaWelcomeOverlay::Create() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = DotaWelcomeOverlay::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = DOTA_WELCOME_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width_) / 2;
    int y = (screenH - height_) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        DOTA_WELCOME_CLASS,
        L"Bad Piggies Welcome",
        WS_POPUP,
        x, y, width_, height_,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    return (hwnd_ != NULL);
}

void DotaWelcomeOverlay::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = NULL;
    }
}

void DotaWelcomeOverlay::Show(bool show) {
    if (!hwnd_) Create();
    visible_ = show;
    if (show) {
        // Re-center if screen resolution changed
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int x = (screenW - width_) / 2;
        int y = (screenH - height_) / 2;
        SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width_, height_, SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd_);
        InvalidateRect(hwnd_, NULL, TRUE);
    } else {
        ShowWindow(hwnd_, SW_HIDE);
    }
}

void DotaWelcomeOverlay::HandleMouseMove(int x, int y) {
    // "ПРИНИМАЮ!" Button rect: centered at Y=345, width=280, height=48
    int btnX = (width_ - 280) / 2;
    int btnY = 348;
    bool inside = (x >= btnX && x <= btnX + 280 && y >= btnY && y <= btnY + 48);

    if (inside != btnHovered_) {
        btnHovered_ = inside;
        InvalidateRect(hwnd_, NULL, FALSE);
    }
}

void DotaWelcomeOverlay::HandleClick(int x, int y) {
    // 1. Close button in header [✕] (X: width_ - 45, Y: 8, 35x30)
    if (x >= width_ - 45 && x <= width_ - 10 && y >= 8 && y <= 38) {
        Show(false);
        return;
    }

    // 2. "ПРИНИМАЮ!" Button (btnX, 348, 280, 48)
    int btnX = (width_ - 280) / 2;
    int btnY = 348;
    if (x >= btnX && x <= btnX + 280 && y >= btnY && y <= btnY + 48) {
        Show(false);
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetFriendlyChat());
        return;
    }
}

void DotaWelcomeOverlay::Render(HDC hdc) {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    Bitmap memBmp(w, h, PixelFormat32bppARGB);
    Graphics g(&memBmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // 1. Deep Dota 2 dark background (#13171d)
    SolidBrush bgBrush(Color(255, 19, 23, 29));
    g.FillRectangle(&bgBrush, 0, 0, w, h);

    // 2. Outer slate/metallic border (#3a4350)
    Pen borderPen(Color(255, 65, 75, 90), 2.0f);
    g.DrawRectangle(&borderPen, 1, 1, w - 2, h - 2);

    // 3. Header bar (#1d232c)
    SolidBrush headerBg(Color(255, 29, 35, 44));
    g.FillRectangle(&headerBg, 2, 2, w - 4, 46);

    // Red Dota 2 accent stripe (#c23c2a)
    SolidBrush redStripe(Color(255, 194, 60, 42));
    g.FillRectangle(&redStripe, 2, 47, w - 4, 3);

    // Header Title
    Gdiplus::Font headerFont(L"Segoe UI", 12.0f, FontStyleBold, UnitPoint);
    SolidBrush goldBrush(Color(255, 235, 215, 150));
    g.DrawString(L"BAD PIGGIES WIDGET — ПРАВИЛА И УПРАВЛЕНИЕ", -1, &headerFont, PointF(22.0f, 13.0f), &goldBrush);

    // Close button [✕]
    SolidBrush closeBg(Color(255, 42, 49, 59));
    g.FillRectangle(&closeBg, w - 45, 9, 35, 28);
    SolidBrush closeText(Color(255, 200, 200, 200));
    g.DrawString(L"✕", -1, &headerFont, PointF((float)(w - 34), 12.0f), &closeText);

    // Fonts for content
    Gdiplus::Font titleFont(L"Segoe UI", 11.0f, FontStyleBold, UnitPoint);
    Gdiplus::Font badgeFont(L"Segoe UI", 10.0f, FontStyleBold, UnitPoint);
    Gdiplus::Font textFont(L"Segoe UI", 9.5f, FontStyleRegular, UnitPoint);
    Gdiplus::Font hintFont(L"Segoe UI", 8.5f, FontStyleItalic, UnitPoint);
    SolidBrush whiteBrush(Color(255, 245, 245, 245));
    SolidBrush textDim(Color(255, 190, 195, 205));
    SolidBrush goldDim(Color(255, 210, 180, 110));

    // --- Block 1: Перемещение свинки ---
    int card1Y = 65;
    SolidBrush cardBg(Color(255, 26, 32, 40));
    Pen cardBorder(Color(255, 48, 56, 68), 1.0f);
    g.FillRectangle(&cardBg, 20, card1Y, w - 40, 75);
    g.DrawRectangle(&cardBorder, 20, card1Y, w - 40, 75);

    // Key badge [Left Ctrl] + [ЛКМ]
    SolidBrush badgeBg(Color(255, 38, 70, 110));
    Pen badgeBorder(Color(255, 70, 130, 200), 1.2f);
    g.FillRectangle(&badgeBg, 32, card1Y + 12, 160, 26);
    g.DrawRectangle(&badgeBorder, 32, card1Y + 12, 160, 26);
    SolidBrush badgeText(Color(255, 210, 235, 255));
    g.DrawString(L"Left Ctrl + ЛКМ", -1, &badgeFont, PointF(48.0f, (float)(card1Y + 15)), &badgeText);

    g.DrawString(L"ПЕРЕМЕЩЕНИЕ СВИНКИ ПО ЭКРАНУ", -1, &titleFont, PointF(205.0f, (float)(card1Y + 14)), &goldBrush);
    g.DrawString(L"Зажмите левый Ctrl и тащите свинку левой кнопкой мыши в любое удобное место.", -1, &textFont, PointF(35.0f, (float)(card1Y + 45)), &textDim);

    // --- Block 2: Настройки в игре ---
    int card2Y = 152;
    g.FillRectangle(&cardBg, 20, card2Y, w - 40, 75);
    g.DrawRectangle(&cardBorder, 20, card2Y, w - 40, 75);

    // Key badge [Навести + Right Shift]
    SolidBrush badge2Bg(Color(255, 110, 75, 25));
    Pen badge2Border(Color(255, 210, 145, 50), 1.2f);
    g.FillRectangle(&badge2Bg, 32, card2Y + 12, 160, 26);
    g.DrawRectangle(&badge2Border, 32, card2Y + 12, 160, 26);
    SolidBrush badge2Text(Color(255, 255, 230, 180));
    g.DrawString(L"Навести + Right Shift", -1, &badgeFont, PointF(36.0f, (float)(card2Y + 15)), &badge2Text);

    g.DrawString(L"БЫСТРЫЕ НАСТРОЙКИ В ДОТЕ", -1, &titleFont, PointF(205.0f, (float)(card2Y + 14)), &goldBrush);
    g.DrawString(L"Наведите мышь на свинку и нажмите правый Shift — откроется меню со скинами и звуками.", -1, &textFont, PointF(35.0f, (float)(card2Y + 45)), &textDim);

    // --- Block 3: Настройка видео Dota 2 ---
    int card3Y = 239;
    g.FillRectangle(&cardBg, 20, card3Y, w - 40, 85);
    g.DrawRectangle(&cardBorder, 20, card3Y, w - 40, 85);

    SolidBrush badge3Bg(Color(255, 90, 35, 30));
    Pen badge3Border(Color(255, 190, 75, 60), 1.2f);
    g.FillRectangle(&badge3Bg, 32, card3Y + 12, 160, 26);
    g.DrawRectangle(&badge3Border, 32, card3Y + 12, 160, 26);
    SolidBrush badge3Text(Color(255, 255, 210, 200));
    g.DrawString(L"В окне без рамки", -1, &badgeFont, PointF(44.0f, (float)(card3Y + 15)), &badge3Text);

    g.DrawString(L"ОБЯЗАТЕЛЬНЫЙ РЕЖИМ ВИДЕО", -1, &titleFont, PointF(205.0f, (float)(card3Y + 14)), &goldBrush);
    g.DrawString(L"В настройках видео Доты выберите: «В окне без рамки» (Borderless Window).", -1, &textFont, PointF(35.0f, (float)(card3Y + 44)), &whiteBrush);
    g.DrawString(L"Клавиши: F5 — TikTok Мемы / Чат | F6 — Что купить? | F7 — Что качать? | F8 — Анекдот", -1, &hintFont, PointF(35.0f, (float)(card3Y + 64)), &goldDim);

    // --- Button: «ПРИНИМАЮ!» (Authentic Dota 2 Accept Match button) ---
    int btnW = 280;
    int btnH = 48;
    int btnX = (w - btnW) / 2;
    int btnY = 348;

    Color btnTop = btnHovered_ ? Color(255, 65, 135, 50) : Color(255, 48, 102, 38);
    Color btnBottom = btnHovered_ ? Color(255, 42, 90, 32) : Color(255, 30, 68, 24);
    LinearGradientBrush btnGrad(Rect(btnX, btnY, btnW, btnH), btnTop, btnBottom, LinearGradientModeVertical);
    g.FillRectangle(&btnGrad, btnX, btnY, btnW, btnH);

    // Glowing green border
    Pen btnBorder(btnHovered_ ? Color(255, 110, 235, 85) : Color(255, 80, 185, 60), 2.0f);
    g.DrawRectangle(&btnBorder, btnX, btnY, btnW, btnH);

    // Button Text: «ПРИНИМАЮ!»
    Gdiplus::Font btnFont(L"Segoe UI", 13.0f, FontStyleBold, UnitPoint);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    SolidBrush btnTextBrush(Color(255, 255, 255, 255));
    RectF btnTextRect((float)btnX, (float)btnY, (float)btnW, (float)btnH);
    g.DrawString(L"ПРИНИМАЮ!", -1, &btnFont, btnTextRect, &sf, &btnTextBrush);

    // BitBlt to screen
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP hBmp = NULL;
    memBmp.GetHBITMAP(Color(0, 0, 0, 0), &hBmp);
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
}

LRESULT CALLBACK DotaWelcomeOverlay::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DotaWelcomeOverlay* self = (DotaWelcomeOverlay*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (self) self->Render(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (self) {
            int x = (short)LOWORD(lParam);
            int y = (short)HIWORD(lParam);
            self->HandleMouseMove(x, y);

            TRACKMOUSEEVENT tme = { 0 };
            tme.cbSize = sizeof(TRACKMOUSEEVENT);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (self) {
            self->btnHovered_ = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (self) {
            int x = (short)LOWORD(lParam);
            int y = (short)HIWORD(lParam);
            self->HandleClick(x, y);
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_SPACE) {
            if (self) self->Show(false);
            return 0;
        }
        break;
    }
    case WM_DESTROY: {
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
