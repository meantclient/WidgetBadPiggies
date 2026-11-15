#include "common.h"
#include "config.h"
#include "overlay.h"
#include "vignette.h"
#include "settings_gui.h"
#include "dota_settings_gui.h"
#include "dota_welcome.h"
#include "voice_assistant.h"
#include "tray.h"
#include "dota_gsi.h"
#include "updater.h"

#define WM_TRAYICON (WM_USER + 100)

static const wchar_t* MAIN_MSG_CLASS = L"BadPiggiesMainMsgClass";
static HWND g_hMsgWnd = NULL;

LRESULT CALLBACK MainMsgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        TrayManager::Instance().HandleTrayMessage(lParam);
        return 0;
    }
    else if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Single instance mutex
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"BadPiggiesDotaWidgetMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // App is already running
        HWND hExisting = FindWindowW(NULL, L"Bad Piggies Widget — Настройки");
        if (hExisting) {
            ShowWindow(hExisting, SW_SHOW);
            SetForegroundWindow(hExisting);
        }
        return 0;
    }

    // Load configuration
    ConfigManager::Load();

    // Register hidden message-only window
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MainMsgWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_MSG_CLASS;
    RegisterClassExW(&wc);

    g_hMsgWnd = CreateWindowExW(0, MAIN_MSG_CLASS, L"BadPiggiesMessageWindow", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    // Initialize Tray Icon
    TrayManager::Instance().Init(g_hMsgWnd);

    // Initialize Overlay & Vignette
    OverlayWindow::Instance().Create();
    VignetteWindow::Instance().Create();

    // Initialize Settings Window, In-game Dota Settings & Welcome Modal
    SettingsWindow::Instance().Create();
    DotaSettingsOverlay::Instance().Create();
    DotaWelcomeOverlay::Instance().Create();

    // Check if initial visibility should hide
    if (ConfigManager::Get().hideWhenNoDota && !DotaGsiService::Instance().IsDotaProcessRunning()) {
        OverlayWindow::Instance().Show(false);
    }

    // Auto-setup Dota 2 GSI config on startup so friend doesn't need to configure anything manually
    std::wstring dummyGsiMsg;
    DotaGsiService::Instance().SetupDotaConfig(dummyGsiMsg);

    // Setup Dota 2 GSI callbacks
    DotaGsiService& gsi = DotaGsiService::Instance();
    gsi.SetOnKill([](int kills, int streak) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerKill(kills, streak);
    });

    gsi.SetOnDeath([]() {
        OverlayWindow::Instance().Show(true);
        VignetteWindow::Instance().SetActive(false);
        OverlayWindow::Instance().TriggerDeath();
    });

    gsi.SetOnLowHp([](bool isLow, int hpPercent) {
        VignetteWindow::Instance().SetActive(isLow);
        OverlayWindow::Instance().TriggerLowHp(isLow, hpPercent);
    });

    gsi.SetOnLotus([](bool isTurbo) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerLotus(isTurbo);
    });

    gsi.SetOnLotusPre([](bool isTurbo) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerLotusPre(isTurbo);
    });

    static bool welcomedThisSession = false;
    gsi.SetOnDotaState([](bool isRunning) {
        SettingsWindow::Instance().UpdateDotaStatus(isRunning);
        if (isRunning && !welcomedThisSession) {
            welcomedThisSession = true;
            DotaWelcomeOverlay::Instance().Show(true);
        }
        if (ConfigManager::Get().hideWhenNoDota) {
            OverlayWindow::Instance().Show(isRunning);
            if (!isRunning) {
                VignetteWindow::Instance().SetActive(false);
            }
        }
    });

    // Start GSI Server & Dota monitor
    gsi.Start(4000);

    // Start Voice Assistant if enabled
    if (ConfigManager::Get().enableVoiceAssistant) {
        VoiceAssistant::Instance().Start();
    }

    // If Dota is already running when widget starts, show welcome popup
    if (gsi.IsDotaProcessRunning()) {
        welcomedThisSession = true;
        DotaWelcomeOverlay::Instance().Show(true);
    }

    // Auto-check for updates silently in background
    if (ConfigManager::Get().autoUpdate) {
        Updater::Instance().CheckForUpdatesAsync(true);
    }

    // On start, show the Settings window so user can choose their pig
    SettingsWindow::Instance().Show(true);
    SettingsWindow::Instance().UpdateDotaStatus(gsi.IsDotaProcessRunning());

    TrayManager::Instance().ShowNotification(
        L"Bad Piggies Widget",
        L"Виджет запущен! Свинка реагирует на киллы и предупреждает о лоу ХП."
    );

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    VoiceAssistant::Instance().Stop();
    gsi.Stop();
    TrayManager::Instance().Remove();
    OverlayWindow::Instance().Destroy();
    VignetteWindow::Instance().Destroy();
    DotaSettingsOverlay::Instance().Destroy();
    DotaWelcomeOverlay::Instance().Destroy();

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return (int)msg.wParam;
}
