#pragma once
#include "common.h"

class DotaWelcomeOverlay {
public:
    static DotaWelcomeOverlay& Instance();

    bool Create();
    void Destroy();
    void Show(bool show);
    bool IsVisible() const { return visible_; }

private:
    DotaWelcomeOverlay();
    ~DotaWelcomeOverlay();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void Render(HDC hdc);
    void HandleClick(int x, int y);
    void HandleMouseMove(int x, int y);

    HWND hwnd_ = NULL;
    bool visible_ = false;
    bool btnHovered_ = false;

    int width_ = 620;
    int height_ = 430;
};
