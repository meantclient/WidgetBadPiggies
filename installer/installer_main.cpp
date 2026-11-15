#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

#define IDI_APP_ICON 101
#define IDR_BUNDLE_PAYLOAD 200

#define IDC_BTN_INSTALL 1001
#define IDC_LBL_STATUS  1002

static HWND g_hMainWnd = NULL;
static HWND g_hBtnInstall = NULL;
static HWND g_hLblStatus = NULL;
static HFONT g_hFontTitle = NULL;
static HFONT g_hFontText = NULL;
static HFONT g_hFontBold = NULL;

static bool CreateShortcut(const std::wstring& targetExe, const std::wstring& shortcutPath, const std::wstring& iconPath) {
    CoInitialize(NULL);
    IShellLinkW* psl = NULL;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&psl);
    if (SUCCEEDED(hr)) {
        psl->SetPath(targetExe.c_str());
        std::wstring workDir = targetExe.substr(0, targetExe.find_last_of(L"\\/"));
        psl->SetWorkingDirectory(workDir.c_str());
        if (!iconPath.empty()) {
            psl->SetIconLocation(iconPath.c_str(), 0);
        }
        IPersistFile* ppf = NULL;
        hr = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (SUCCEEDED(hr)) {
            hr = ppf->Save(shortcutPath.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    return SUCCEEDED(hr);
}

static std::wstring FindDotaCfgPath() {
    HKEY hKey;
    wchar_t steamPath[MAX_PATH] = { 0 };
    DWORD size = sizeof(steamPath);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"SteamPath", NULL, NULL, (LPBYTE)steamPath, &size);
        RegCloseKey(hKey);
    }

    std::wstring standardPath = std::wstring(steamPath) + L"\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg";
    for (auto& ch : standardPath) if (ch == L'/') ch = L'\\';
    if (PathFileExistsW(standardPath.c_str())) return standardPath;

    // Check libraryfolders.vdf
    if (wcslen(steamPath) > 0) {
        std::wstring libVdf = std::wstring(steamPath) + L"\\steamapps\\libraryfolders.vdf";
        for (auto& ch : libVdf) if (ch == L'/') ch = L'\\';

        std::wifstream vdfFile(libVdf);
        if (vdfFile.is_open()) {
            std::wstring line;
            while (std::getline(vdfFile, line)) {
                size_t p = line.find(L"\"path\"");
                if (p != std::wstring::npos) {
                    size_t firstQuote = line.find(L"\"", p + 6);
                    if (firstQuote != std::wstring::npos) {
                        size_t secondQuote = line.find(L"\"", firstQuote + 1);
                        if (secondQuote != std::wstring::npos) {
                            std::wstring libPath = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                            std::wstring cleanPath;
                            for (size_t i = 0; i < libPath.length(); ++i) {
                                if (libPath[i] == L'\\' && i + 1 < libPath.length() && libPath[i + 1] == L'\\') {
                                    continue;
                                }
                                cleanPath += libPath[i];
                            }
                            std::wstring checkDota = cleanPath + L"\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg";
                            if (PathFileExistsW(checkDota.c_str())) {
                                return checkDota;
                            }
                        }
                    }
                }
            }
        }
    }

    // Check all drive letters (C: through Z:)
    for (wchar_t drive = L'C'; drive <= L'Z'; ++drive) {
        std::wstring driveRoot = std::wstring(1, drive) + L":";
        const wchar_t* subPaths[] = {
            L"\\Steam\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg",
            L"\\SteamLibrary\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg",
            L"\\Games\\Steam\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg",
            L"\\Games\\SteamLibrary\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg",
            L"\\Program Files (x86)\\Steam\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg",
            L"\\Program Files\\Steam\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg"
        };
        for (const auto* sub : subPaths) {
            std::wstring fullCheck = driveRoot + sub;
            if (PathFileExistsW(fullCheck.c_str())) return fullCheck;
        }
    }

    return L"";
}

static void InstallDotaGsi() {
    std::wstring dotaCfg = FindDotaCfgPath();
    if (dotaCfg.empty()) return;

    std::string content =
        "\"Dota 2 Integration Configuration\"\n"
        "{\n"
        "    \"uri\"           \"http://127.0.0.1:4000/\"\n"
        "    \"timeout\"       \"5.0\"\n"
        "    \"buffer\"        \"0.1\"\n"
        "    \"throttle\"      \"0.1\"\n"
        "    \"heartbeat\"     \"30.0\"\n"
        "    \"data\"\n"
        "    {\n"
        "        \"provider\"      \"1\"\n"
        "        \"map\"           \"1\"\n"
        "        \"player\"        \"1\"\n"
        "        \"hero\"          \"1\"\n"
        "        \"abilities\"     \"1\"\n"
        "        \"items\"         \"1\"\n"
        "        \"buildings\"     \"1\"\n"
        "        \"draft\"         \"1\"\n"
        "        \"wearables\"     \"1\"\n"
        "    }\n"
        "}\n";

    std::wstring gsiSubDir = dotaCfg + L"\\gamestate_integration";
    SHCreateDirectoryExW(NULL, gsiSubDir.c_str(), NULL);
    std::wstring cfgFile = gsiSubDir + L"\\gamestate_integration_badpigs.cfg";

    std::ofstream out(cfgFile, std::ios::binary);
    if (out.is_open()) {
        out.write(content.c_str(), content.size());
        out.close();
    }
}

static void EnsureParentDir(const std::wstring& filePath) {
    size_t pos = filePath.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        std::wstring dir = filePath.substr(0, pos);
        SHCreateDirectoryExW(NULL, dir.c_str(), NULL);
    }
}

static void RunInstallation(HWND hwnd) {
    if (g_hLblStatus) SetWindowTextW(g_hLblStatus, L"Подготовка файлов установки...");
    if (g_hBtnInstall) EnableWindow(g_hBtnInstall, FALSE);

    // 1. Get embedded binary bundle payload
    HMODULE hMod = GetModuleHandle(NULL);
    HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(IDR_BUNDLE_PAYLOAD), RT_RCDATA);
    if (!hRes) {
        if (hwnd) MessageBoxW(hwnd, L"Не удалось найти встроенные компоненты виджета!", L"Ошибка", MB_OK | MB_ICONERROR);
        if (g_hBtnInstall) EnableWindow(g_hBtnInstall, TRUE);
        return;
    }

    HGLOBAL hGlobal = LoadResource(hMod, hRes);
    const unsigned char* pData = (const unsigned char*)LockResource(hGlobal);
    DWORD dataSize = SizeofResource(hMod, hRes);

    if (dataSize < 8 || memcmp(pData, "BPKG", 4) != 0) {
        if (hwnd) MessageBoxW(hwnd, L"Поврежден установочный пакет!", L"Ошибка", MB_OK | MB_ICONERROR);
        if (g_hBtnInstall) EnableWindow(g_hBtnInstall, TRUE);
        return;
    }

    // 2. Target folder: %LOCALAPPDATA%\BadPiggiesWidget
    wchar_t localApp[MAX_PATH] = { 0 };
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localApp);
    std::wstring targetDir = std::wstring(localApp) + L"\\BadPiggiesWidget";
    SHCreateDirectoryExW(NULL, targetDir.c_str(), NULL);

    // Terminate any running widget instance so files can be overwritten cleanly
    _wsystem(L"taskkill /f /im BadPiggiesDotaWidget.exe >nul 2>&1");
    Sleep(800);

    if (g_hLblStatus) SetWindowTextW(g_hLblStatus, L"Копирование компонентов программы...");

    // 3. Extract files directly from memory without powershell or temporary zip
    size_t offset = 4;
    uint32_t fileCount = *(const uint32_t*)(pData + offset);
    offset += 4;

    for (uint32_t i = 0; i < fileCount && offset < dataSize; ++i) {
        uint16_t pathLen = *(const uint16_t*)(pData + offset);
        offset += 2;

        std::string relPathA((const char*)(pData + offset), pathLen);
        offset += pathLen;

        uint32_t fileLen = *(const uint32_t*)(pData + offset);
        offset += 4;

        const unsigned char* fileBytes = pData + offset;
        offset += fileLen;

        int wLen = MultiByteToWideChar(CP_UTF8, 0, relPathA.c_str(), (int)relPathA.size(), NULL, 0);
        std::wstring relPathW(wLen, 0);
        MultiByteToWideChar(CP_UTF8, 0, relPathA.c_str(), (int)relPathA.size(), &relPathW[0], wLen);

        std::wstring fullPath = targetDir + L"\\" + relPathW;
        EnsureParentDir(fullPath);

        HANDLE hOut = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOut == INVALID_HANDLE_VALUE) {
            _wsystem(L"taskkill /f /im BadPiggiesDotaWidget.exe >nul 2>&1");
            Sleep(500);
            hOut = CreateFileW(fullPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hOut, fileBytes, fileLen, &written, NULL);
            CloseHandle(hOut);
        }
    }

    // 4. Install Dota GSI config
    if (g_hLblStatus) SetWindowTextW(g_hLblStatus, L"Настройка интеграции с Dota 2...");
    InstallDotaGsi();

    // 5. Create Desktop shortcut
    if (g_hLblStatus) SetWindowTextW(g_hLblStatus, L"Создание ярлыка на Рабочем столе...");
    wchar_t desktop[MAX_PATH] = { 0 };
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop);
    std::wstring targetExe = targetDir + L"\\BadPiggiesDotaWidget.exe";
    std::wstring shortcut = std::wstring(desktop) + L"\\Bad Piggies Widget.lnk";
    CreateShortcut(targetExe, shortcut, targetExe);

    // 6. Create Start Menu shortcut
    wchar_t startMenu[MAX_PATH] = { 0 };
    SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startMenu);
    std::wstring smShortcut = std::wstring(startMenu) + L"\\Bad Piggies Widget.lnk";
    CreateShortcut(targetExe, smShortcut, targetExe);

    // 7. Launch widget
    if (g_hLblStatus) SetWindowTextW(g_hLblStatus, L"Запуск виджета...");
    ShellExecuteW(NULL, L"open", targetExe.c_str(), NULL, targetDir.c_str(), SW_SHOW);

    if (hwnd) {
        MessageBoxW(hwnd,
            L"Bad Piggies Widget успешно установлен!\n\n"
            L"1. Интеграция с Dota 2 настроена автоматически.\n"
            L"2. Ярлык создан на Рабочем столе.\n"
            L"3. В Dota 2 в настройках видео выберите: «В окне без рамки» (Borderless Window).\n\n"
            L"Свинка уже запущена. Приятной игры!",
            L"Установка завершена",
            MB_OK | MB_ICONINFORMATION);
        DestroyWindow(hwnd);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDC_BTN_INSTALL) {
            RunInstallation(hwnd);
            return 0;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(225, 230, 240));
        SetBkColor(hdc, RGB(28, 33, 40));
        static HBRUSH hbrStatic = CreateSolidBrush(RGB(28, 33, 40));
        return (LRESULT)hbrStatic;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    // Check for silent update command
    if (lpCmd && (strstr(lpCmd, "/update") || strstr(lpCmd, "/silent") || strstr(lpCmd, "-update") || strstr(lpCmd, "--update"))) {
        RunInstallation(NULL);
        return 0;
    }

    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"BadPiggiesSetupClass";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(28, 33, 40));
    RegisterClassExW(&wc);

    int winW = 500;
    int winH = 340;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - winW) / 2;
    int posY = (screenH - winH) / 2;

    g_hMainWnd = CreateWindowExW(
        0, L"BadPiggiesSetupClass",
        L"Установка — Bad Piggies Dota 2 Widget",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        posX, posY, winW, winH,
        NULL, NULL, hInstance, NULL
    );

    g_hFontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_hFontText = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_hFontBold = CreateFontW(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HWND lblTitle = CreateWindowW(L"STATIC", L"Bad Piggies Widget для Dota 2",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 25, 445, 30, g_hMainWnd, NULL, hInstance, NULL);
    SendMessageW(lblTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

    HWND lblDesc = CreateWindowW(L"STATIC",
        L"Установщик автоматически установит виджет со свинками,\n"
        L"настроит интеграцию с Dota 2 (GSI), добавит озвучку,\n"
        L"шрифт и создаст удобный ярлык на вашем Рабочем столе.",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        25, 75, 435, 65, g_hMainWnd, NULL, hInstance, NULL);
    SendMessageW(lblDesc, WM_SETFONT, (WPARAM)g_hFontText, TRUE);

    g_hBtnInstall = CreateWindowW(L"BUTTON", L"УСТАНОВИТЬ И ЗАПУСТИТЬ",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        110, 160, 265, 48, g_hMainWnd, (HMENU)IDC_BTN_INSTALL, hInstance, NULL);
    SendMessageW(g_hBtnInstall, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    g_hLblStatus = CreateWindowW(L"STATIC", L"Нажмите кнопку для быстрой установки в один клик",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 235, 445, 25, g_hMainWnd, (HMENU)IDC_LBL_STATUS, hInstance, NULL);
    SendMessageW(g_hLblStatus, WM_SETFONT, (WPARAM)g_hFontText, TRUE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DeleteObject(g_hFontTitle);
    DeleteObject(g_hFontText);
    DeleteObject(g_hFontBold);

    return (int)msg.wParam;
}
