#pragma once
#include "common.h"

class SettingsWindow {
public:
    static SettingsWindow& Instance();

    bool Create();
    void Show(bool show);
    bool IsVisible() const { return visible_; }
    void UpdateDotaStatus(bool isRunning);

    HWND GetHwnd() const { return hwnd_; }

private:
    SettingsWindow();
    ~SettingsWindow();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void ApplySettings();
    void LoadCurrentSettingsToUI();
    void OnPigSelectionChanged();
    void DrawPreview(HDC hdc, RECT rc);

    HWND hwnd_ = NULL;
    bool visible_ = false;

    // Controls
    HWND comboPigs_ = NULL;
    HWND comboSize_ = NULL;
    HWND chkSound_ = NULL;
    HWND chkHide_ = NULL;
    HWND chkAutostart_ = NULL;
    HWND chkUseName_ = NULL;
    HWND txtPlayerName_ = NULL;
    HWND chkVoiceTTS_ = NULL;
    HWND comboVoiceGender_ = NULL;
    HWND chkToxicMode_ = NULL;
    HWND chkVoiceAssistant_ = NULL;
    HWND txtWakeWord_ = NULL;
    HWND chkProactiveAdvisor_ = NULL;

    HWND btnTestKill_ = NULL;
    HWND btnTestDeath_ = NULL;
    HWND btnTestLowHp_ = NULL;
    HWND btnTestAdvisor_ = NULL;
    HWND btnSetupDota_ = NULL;
    HWND btnCheckUpdate_ = NULL;
    HWND btnSave_ = NULL;
    HWND lblStatus_ = NULL;
    HWND previewBox_ = NULL;

    HFONT hFont_ = NULL;
    HFONT hFontBold_ = NULL;

    std::unique_ptr<Gdiplus::Image> previewImage_;
    int previewPigType_ = 1;
};
