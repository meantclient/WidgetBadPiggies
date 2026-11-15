#pragma once
#include "common.h"
#include <string>

class Updater {
public:
    static Updater& Instance();

    void CheckForUpdatesAsync(bool silent, HWND hNotifyWnd = NULL);
    bool CheckForUpdatesSync(bool silent, HWND hNotifyWnd = NULL);

private:
    Updater();
    ~Updater();
};
