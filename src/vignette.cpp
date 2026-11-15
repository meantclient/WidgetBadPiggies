#include "vignette.h"
#include "config.h"
#include "dota_gsi.h"
#include <cmath>

using namespace Gdiplus;

static const wchar_t* VIGNETTE_CLASS = L"BadPiggiesVignetteClass";

VignetteWindow& VignetteWindow::Instance() {
    static VignetteWindow instance;
    return instance;
}

VignetteWindow::VignetteWindow() {}

VignetteWindow::~VignetteWindow() {
    Destroy();
}

bool VignetteWindow::Create() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = VignetteWindow::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = VIGNETTE_CLASS;
    RegisterClassExW(&wc);

    screenW_ = GetSystemMetrics(SM_CXSCREEN);
    screenH_ = GetSystemMetrics(SM_CYSCREEN);

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        VIGNETTE_CLASS,
        L"Bad Piggies Vignette",
        WS_POPUP,
        0, 0, screenW_, screenH_,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    if (!hwnd_) return false;

    InitBitmap();
    SetTimer(hwnd_, 1, 16, NULL); // ~60 FPS
    ShowWindow(hwnd_, SW_HIDE);
    return true;
}

void VignetteWindow::Destroy() {
    if (hwnd_) {
        KillTimer(hwnd_, 1);
        DestroyWindow(hwnd_);
        hwnd_ = NULL;
    }
    if (hdcMem_) {
        if (hOldBitmap_) SelectObject(hdcMem_, hOldBitmap_);
        DeleteDC(hdcMem_);
        hdcMem_ = NULL;
    }
    if (hBitmap_) {
        DeleteObject(hBitmap_);
        hBitmap_ = NULL;
    }
}

void VignetteWindow::InitBitmap() {
    std::wstring imgPath = ConfigManager::GetAssetPath(L"vignette.png");
    Image img(imgPath.c_str());

    HDC hdcScreen = GetDC(NULL);
    hdcMem_ = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = screenW_;
    bmi.bmiHeader.biHeight = -screenH_; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = nullptr;
    hBitmap_ = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0);
    hOldBitmap_ = (HBITMAP)SelectObject(hdcMem_, hBitmap_);
    ReleaseDC(NULL, hdcScreen);

    Bitmap bmp(screenW_, screenH_, screenW_ * 4, PixelFormat32bppPARGB, (BYTE*)pvBits);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.Clear(Color(0, 0, 0, 0));

    if (img.GetLastStatus() == Ok) {
        g.DrawImage(&img, RectF(0, 0, (float)screenW_, (float)screenH_));
    }
}

void VignetteWindow::SetActive(bool active) {
    if (active && !ConfigManager::Get().enableVignette) {
        active = false;
    }
    active_ = active;
    if (active && hwnd_) {
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

LRESULT CALLBACK VignetteWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    VignetteWindow* self = (VignetteWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return 0;
    }
    case WM_TIMER:
        if (self) self->OnTimer();
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void VignetteWindow::OnTimer() {
    if (!hwnd_ || !hdcMem_) return;

    if (active_ && !DotaGsiService::Instance().IsDotaActive() && ConfigManager::Get().hideWhenNoDota) {
        active_ = false;
    }

    if (active_) {
        pulseTimer_ += 0.05;
        // Pulsing sine wave between ~60 and ~230 alpha
        double wave = (sin(pulseTimer_ * 4.5) * 0.5 + 0.5);
        currentAlpha_ = (float)(wave * 170.0 + 60.0);
    } else {
        if (currentAlpha_ > 0.0f) {
            currentAlpha_ -= 12.0f;
            if (currentAlpha_ <= 0.0f) {
                currentAlpha_ = 0.0f;
                ShowWindow(hwnd_, SW_HIDE);
                return;
            }
        } else {
            return;
        }
    }

    static int topmostCounter = 0;
    if (++topmostCounter >= 30) {
        topmostCounter = 0;
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    HDC hdcScreen = GetDC(NULL);
    POINT ptDst = { 0, 0 };
    POINT ptSrc = { 0, 0 };
    SIZE size = { screenW_, screenH_ };

    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = (BYTE)currentAlpha_;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd_, hdcScreen, &ptDst, &size, hdcMem_, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(NULL, hdcScreen);
}
