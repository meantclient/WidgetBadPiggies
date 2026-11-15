#include "tray.h"
#include "settings_gui.h"
#include "overlay.h"
#include "config.h"
#include "resource.h"

#define WM_TRAYICON (WM_USER + 100)

enum TrayMenuIDs {
    ID_TRAY_SETTINGS = 2001,
    ID_TRAY_TEST_KILL = 2002,
    ID_TRAY_TEST_DEATH = 2003,
    ID_TRAY_EXIT = 2004
};

TrayManager& TrayManager::Instance() {
    static TrayManager instance;
    return instance;
}

TrayManager::TrayManager() {}

TrayManager::~TrayManager() {
    Remove();
}

bool TrayManager::Init(HWND hwndParent) {
    hwndParent_ = hwndParent;

    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwndParent;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    HICON hIcon = LoadIconW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!hIcon) {
        hIcon = (HICON)LoadImageW(NULL, ConfigManager::GetAssetPath(L"app.ico").c_str(), IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
    }
    if (!hIcon) hIcon = LoadIcon(NULL, IDI_APPLICATION);
    nid_.hIcon = hIcon;
    wcscpy_s(nid_.szTip, L"Bad Piggies — Dota 2 Widget");

    isAdded_ = Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
    return isAdded_;
}

void TrayManager::Remove() {
    if (isAdded_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        isAdded_ = false;
    }
}

void TrayManager::ShowNotification(const wchar_t* title, const wchar_t* msg) {
    if (!isAdded_) return;

    NOTIFYICONDATAW nid = nid_;
    nid.uFlags |= NIF_INFO;
    wcscpy_s(nid.szInfoTitle, title);
    wcscpy_s(nid.szInfo, msg);
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayManager::HandleTrayMessage(LPARAM lParam) {
    if (lParam == WM_LBUTTONDBLCLK) {
        SettingsWindow::Instance().Show(true);
    }
    else if (lParam == WM_RBUTTONUP) {
        POINT pt;
        GetCursorPos(&pt);

        HMENU hMenu = CreatePopupMenu();
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_SETTINGS, L"Настройки виджета");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_TEST_KILL, L"Тест: Килл! (Прыжок)");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_TEST_DEATH, L"Тест: Смерть");
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
        InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Выход");

        SetForegroundWindow(hwndParent_);
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwndParent_, NULL);
        DestroyMenu(hMenu);

        if (cmd == ID_TRAY_SETTINGS) {
            SettingsWindow::Instance().Show(true);
        }
        else if (cmd == ID_TRAY_TEST_KILL) {
            OverlayWindow::Instance().Show(true);
            OverlayWindow::Instance().TriggerKill(1, 0);
        }
        else if (cmd == ID_TRAY_TEST_DEATH) {
            OverlayWindow::Instance().Show(true);
            OverlayWindow::Instance().TriggerDeath();
        }
        else if (cmd == ID_TRAY_EXIT) {
            PostQuitMessage(0);
        }
    }
}
