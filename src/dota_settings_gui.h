#pragma once
#include "common.h"
#include "config.h"

class DotaSettingsOverlay {
public:
    static DotaSettingsOverlay& Instance();

    bool Create();
    void Destroy();
    void Show(bool show);
    void Toggle();
    bool IsVisible() const { return visible_; }

private:
    DotaSettingsOverlay();
    ~DotaSettingsOverlay();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render(HDC hdc);
    void HandleClick(int x, int y);
    void SaveAndClose();

    HWND hwnd_ = NULL;
    HWND editName_ = NULL;
    HWND editWakeWord_ = NULL;
    HFONT hFontEdit_ = NULL;
    bool visible_ = false;

    int width_ = 680;
    int height_ = 760;

    // Temporary config modified in GUI
    AppConfig cfgCopy_;
};
