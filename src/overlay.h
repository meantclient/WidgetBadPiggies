#pragma once
#include "common.h"

enum class PigAnimationState {
    IDLE,
    JUMP_KILL,
    SAD_DEATH,
    PANIC_LOW_HP,
    LOTUS_REMINDER
};

class OverlayWindow {
public:
    static OverlayWindow& Instance();

    bool Create();
    void Destroy();
    void Show(bool show);
    bool IsVisible() const { return visible_; }

    void TriggerKill(int kills = 1, int streak = 0);
    void TriggerDeath();
    void TriggerLowHp(bool isLow, int hpPercent = 0);
    void TriggerLotus(bool isTurbo = false);
    void TriggerLotusPre(bool isTurbo = false);
    void TriggerAdvice(const std::wstring& advice);
    void ReloadSkinAndSize();

    HWND GetHwnd() const { return hwnd_; }

private:
    OverlayWindow();
    ~OverlayWindow();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnTimer();
    void Render();
    void LoadCurrentSkin();
    void LoadCustomFont();
    void PlayAudio(const std::wstring& filename);

    HWND hwnd_ = NULL;
    bool visible_ = false;
    ULONG_PTR gdiplusToken_ = 0;

    std::unique_ptr<Gdiplus::Image> currentPigImage_;
    std::unique_ptr<Gdiplus::Image> beatenPigImage_;
    int loadedPigType_ = -1;

    std::unique_ptr<Gdiplus::PrivateFontCollection> fontCollection_;
    std::wstring customFontName_ = L"";

    // Dimensions
    int winWidth_ = 320;
    int winHeight_ = 340;
    int posX_ = 0;
    int posY_ = 0;

    // Dragging state
    bool isDragging_ = false;
    POINT dragStartMouse_ = { 0, 0 };
    POINT dragStartPos_ = { 0, 0 };

    // Animation state
    PigAnimationState state_ = PigAnimationState::IDLE;
    double animTimer_ = 0.0;
    double stateDuration_ = 0.0;
    double lastLowHpSoundTime_ = 0.0;
    std::wstring bubbleText_ = L"";

    // Speech bubble random selection
    std::vector<std::wstring> killPhrases_;
    std::vector<std::wstring> streakPhrases_;
    std::vector<std::wstring> deathPhrases_;
    std::vector<std::wstring> lowHpPhrases_;
    std::vector<std::wstring> lotusPhrases_;
};
