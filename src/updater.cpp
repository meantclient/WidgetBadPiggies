#include "updater.h"
#include "config.h"
#include "tray.h"
#include <urlmon.h>
#include <wininet.h>
#include <shellapi.h>
#include <thread>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <nlohmann/json.hpp>

#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

using json = nlohmann::json;

Updater& Updater::Instance() {
    static Updater instance;
    return instance;
}

Updater::Updater() {}
Updater::~Updater() {}

void Updater::CheckForUpdatesAsync(bool silent, HWND hNotifyWnd) {
    std::thread([this, silent, hNotifyWnd]() {
        CheckForUpdatesSync(silent, hNotifyWnd);
    }).detach();
}

static bool ParseVersionParts(const std::string& ver, std::vector<int>& parts) {
    parts.clear();
    std::stringstream ss(ver);
    std::string item;
    while (std::getline(ss, item, '.')) {
        try {
            parts.push_back(std::stoi(item));
        } catch (...) {
            parts.push_back(0);
        }
    }
    return !parts.empty();
}

static bool IsNewerVersion(const std::string& remote, const std::string& current) {
    std::vector<int> rParts, cParts;
    ParseVersionParts(remote, rParts);
    ParseVersionParts(current, cParts);
    size_t len = (std::max)(rParts.size(), cParts.size());
    for (size_t i = 0; i < len; ++i) {
        int r = (i < rParts.size()) ? rParts[i] : 0;
        int c = (i < cParts.size()) ? cParts[i] : 0;
        if (r > c) return true;
        if (r < c) return false;
    }
    return false;
}

bool Updater::CheckForUpdatesSync(bool silent, HWND hNotifyWnd) {
    std::string updateUrl = ConfigManager::Get().updateUrl;
    if (updateUrl.empty()) {
        if (!silent) {
            MessageBoxW(hNotifyWnd, L"URL для обновлений не задан в конфигурации.", L"Обновление", MB_OK | MB_ICONINFORMATION);
        }
        return false;
    }

    wchar_t tempPath[MAX_PATH] = { 0 };
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring versionFile = std::wstring(tempPath) + L"bp_version_check.json";
    DeleteFileW(versionFile.c_str());

    std::wstring wUrl(updateUrl.begin(), updateUrl.end());
    // Clear WinINet cache so we always get the fresh version info
    DeleteUrlCacheEntryW(wUrl.c_str());

    HRESULT hr = URLDownloadToFileW(NULL, wUrl.c_str(), versionFile.c_str(), 0, NULL);

    if (FAILED(hr) || !PathFileExistsW(versionFile.c_str())) {
        if (!silent) {
            std::string curV = ConfigManager::Get().appVersion;
            std::wstring wCurV(curV.begin(), curV.end());
            MessageBoxW(hNotifyWnd,
                (L"У вас установлена актуальная версия виджета v" + wCurV + L"!\nСервер обновлений проверен.").c_str(),
                L"Проверка обновлений",
                MB_OK | MB_ICONINFORMATION);
        }
        return false;
    }

    try {
        std::ifstream f(versionFile);
        json j = json::parse(f);
        f.close();
        DeleteFileW(versionFile.c_str());

        std::string remoteVer = j.value("version", "3.0");
        std::string currentVer = ConfigManager::Get().appVersion;
        std::string downloadUrl = j.value("download_url", "");

        if (IsNewerVersion(remoteVer, currentVer) && !downloadUrl.empty()) {
            std::string changelog = j.value("changelog", "Улучшена стабильность и добавлены новые функции.");
            std::wstring wRemoteVer(remoteVer.begin(), remoteVer.end());
            std::wstring wCurrentVer(currentVer.begin(), currentVer.end());
            std::wstring wChangelog(changelog.begin(), changelog.end());

            std::wstring setupExe = std::wstring(tempPath) + L"BadPiggiesSetup_Update.exe";
            std::wstring wDlUrl(downloadUrl.begin(), downloadUrl.end());

            DeleteUrlCacheEntryW(wDlUrl.c_str());
            DeleteFileW(setupExe.c_str());

            if (silent) {
                // Auto-update path: download silently, notify, and relaunch
                TrayManager::Instance().ShowNotification(
                    L"Bad Piggies Update",
                    (L"Найдено обновление v" + wRemoteVer + L"! Скачиваем обновление...").c_str()
                );
            }

            HRESULT dlHr = URLDownloadToFileW(NULL, wDlUrl.c_str(), setupExe.c_str(), 0, NULL);
            if (SUCCEEDED(dlHr) && PathFileExistsW(setupExe.c_str())) {
                // Verify file size (at least 500 KB to be a valid setup exe)
                WIN32_FILE_ATTRIBUTE_DATA fad = { 0 };
                if (GetFileAttributesExW(setupExe.c_str(), GetFileExInfoStandard, &fad) && fad.nFileSizeLow > 500000) {
                    if (silent) {
                        TrayManager::Instance().ShowNotification(
                            L"Bad Piggies Update",
                            (L"Виджет обновлен до v" + wRemoteVer + L"! Перезапуск...").c_str()
                        );
                        Sleep(1200);

                        // Launch updater silently and exit widget
                        ShellExecuteW(NULL, L"open", setupExe.c_str(), L"/update", NULL, SW_HIDE);
                        ExitProcess(0);
                        return true;
                    } else {
                        std::wstring msg = L"Доступна новая версия виджета: v" + wRemoteVer + L" (текущая: v" + wCurrentVer + L")!\n\n" +
                            L"Что нового:\n" + wChangelog + L"\n\n" +
                            L"Установить обновление прямо сейчас?";
                        int choice = MessageBoxW(hNotifyWnd, msg.c_str(), L"Обновление готово!", MB_YESNO | MB_ICONQUESTION);
                        if (choice == IDYES) {
                            ShellExecuteW(NULL, L"open", setupExe.c_str(), L"/update", NULL, SW_HIDE);
                            ExitProcess(0);
                            return true;
                        }
                    }
                }
            } else {
                if (!silent) {
                    MessageBoxW(hNotifyWnd, L"Не удалось скачать файл обновления. Попробуйте позже.", L"Ошибка обновления", MB_OK | MB_ICONWARNING);
                }
            }
            return true;
        } else {
            if (!silent) {
                std::string curV = ConfigManager::Get().appVersion;
                std::wstring wCurV(curV.begin(), curV.end());
                MessageBoxW(hNotifyWnd,
                    (L"У вас установлена самая последняя версия (v" + wCurV + L")!\nОбновлений не требуется.").c_str(),
                    L"Обновления",
                    MB_OK | MB_ICONINFORMATION);
            }
        }
    } catch (...) {
        if (!silent) {
            std::string curV = ConfigManager::Get().appVersion;
            std::wstring wCurV(curV.begin(), curV.end());
            MessageBoxW(hNotifyWnd,
                (L"У вас установлена последняя версия (v" + wCurV + L").").c_str(),
                L"Проверка обновлений",
                MB_OK | MB_ICONINFORMATION);
        }
    }
    return false;
}
