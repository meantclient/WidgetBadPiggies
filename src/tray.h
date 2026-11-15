#pragma once
#include "common.h"
#include <shellapi.h>

class TrayManager {
public:
    static TrayManager& Instance();

    bool Init(HWND hwndParent);
    void Remove();
    void ShowNotification(const wchar_t* title, const wchar_t* msg);
    void HandleTrayMessage(LPARAM lParam);

private:
    TrayManager();
    ~TrayManager();

    HWND hwndParent_ = NULL;
    NOTIFYICONDATAW nid_ = { 0 };
    bool isAdded_ = false;
};
