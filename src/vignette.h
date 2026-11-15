#pragma once
#include "common.h"

class VignetteWindow {
public:
    static VignetteWindow& Instance();

    bool Create();
    void Destroy();
    void SetActive(bool active);
    bool IsActive() const { return active_; }

    HWND GetHwnd() const { return hwnd_; }

private:
    VignetteWindow();
    ~VignetteWindow();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnTimer();
    void InitBitmap();

    HWND hwnd_ = NULL;
    bool active_ = false;
    float currentAlpha_ = 0.0f;
    double pulseTimer_ = 0.0;

    int screenW_ = 1920;
    int screenH_ = 1080;

    HDC hdcMem_ = NULL;
    HBITMAP hBitmap_ = NULL;
    HBITMAP hOldBitmap_ = NULL;
};
