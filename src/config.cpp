#include "config.h"
#include <shlwapi.h>
#include <mmsystem.h>
#include <algorithm>
#include <string>

static std::string WStringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    std::string str(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &str[0], len, NULL, NULL);
    return str;
}

static std::wstring Utf8ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &wstr[0], len);
    return wstr;
}

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winmm.lib")

static AppConfig g_config;
static const wchar_t* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* APP_NAME = L"DotaBadPiggiesWidget";

AppConfig& ConfigManager::Get() {
    return g_config;
}

std::wstring ConfigManager::GetExePath() {
    wchar_t path[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return std::wstring(path);
}

std::wstring ConfigManager::GetExeDir() {
    std::wstring path = GetExePath();
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return path.substr(0, pos);
    }
    return L".";
}

std::wstring ConfigManager::GetAssetPath(const std::wstring& filename) {
    return GetExeDir() + L"\\assets\\" + filename;
}

void ConfigManager::ApplyAudioVolume() {
    // Avoid muting or altering global wave device handles
}

void ConfigManager::Load() {
    std::wstring iniPath = GetExeDir() + L"\\config.ini";

    g_config.pigType = GetPrivateProfileIntW(L"Settings", L"pig_type", 1, iniPath.c_str());
    if (g_config.pigType < 1 || g_config.pigType > 7) g_config.pigType = 1;

    g_config.pigSize = GetPrivateProfileIntW(L"Settings", L"pig_size", 180, iniPath.c_str());
    if (g_config.pigSize < 100 || g_config.pigSize > 350) g_config.pigSize = 180;

    g_config.enableSound = GetPrivateProfileIntW(L"Settings", L"enable_sound", 1, iniPath.c_str()) != 0;
    g_config.soundVolume = GetPrivateProfileIntW(L"Settings", L"sound_volume", 100, iniPath.c_str());
    if (g_config.soundVolume < 0 || g_config.soundVolume > 100) g_config.soundVolume = 100;

    g_config.soundKill = GetPrivateProfileIntW(L"Settings", L"sound_kill", 1, iniPath.c_str()) != 0;
    g_config.soundDeath = GetPrivateProfileIntW(L"Settings", L"sound_death", 1, iniPath.c_str()) != 0;
    g_config.soundLowHp = GetPrivateProfileIntW(L"Settings", L"sound_low_hp", 1, iniPath.c_str()) != 0;
    g_config.soundLotus = GetPrivateProfileIntW(L"Settings", L"sound_lotus", 1, iniPath.c_str()) != 0;
    g_config.enableVoiceTTS = GetPrivateProfileIntW(L"Settings", L"enable_voice_tts", 1, iniPath.c_str()) != 0;

    g_config.enableVignette = GetPrivateProfileIntW(L"Settings", L"enable_vignette", 1, iniPath.c_str()) != 0;
    g_config.enableBeatenPig = GetPrivateProfileIntW(L"Settings", L"enable_beaten_pig", 1, iniPath.c_str()) != 0;
    g_config.enableLotusReminder = GetPrivateProfileIntW(L"Settings", L"enable_lotus_reminder", 1, iniPath.c_str()) != 0;
    g_config.lotusMode = GetPrivateProfileIntW(L"Settings", L"lotus_mode", 0, iniPath.c_str());
    if (g_config.lotusMode < 0 || g_config.lotusMode > 2) g_config.lotusMode = 0;

    g_config.hideWhenNoDota = GetPrivateProfileIntW(L"Settings", L"hide_when_no_dota", 1, iniPath.c_str()) != 0;
    g_config.autostart = IsAutoStartEnabled();

    g_config.posX = GetPrivateProfileIntW(L"Settings", L"pos_x", -1, iniPath.c_str());
    g_config.posY = GetPrivateProfileIntW(L"Settings", L"pos_y", -1, iniPath.c_str());

    g_config.usePlayerName = GetPrivateProfileIntW(L"Settings", L"use_player_name", 1, iniPath.c_str()) != 0;
    wchar_t nameBuf[128] = { 0 };
    GetPrivateProfileStringW(L"Settings", L"player_name", L"Марат", nameBuf, 128, iniPath.c_str());
    g_config.playerName = nameBuf;
    if (g_config.playerName.empty()) g_config.playerName = L"Марат";

    g_config.voiceGender = GetPrivateProfileIntW(L"Settings", L"voice_gender", 1, iniPath.c_str());
    g_config.enableToxicMode = GetPrivateProfileIntW(L"Settings", L"enable_toxic_mode", 0, iniPath.c_str()) != 0;

    g_config.enableVoiceAssistant = GetPrivateProfileIntW(L"Settings", L"enable_voice_assistant", 1, iniPath.c_str()) != 0;
    wchar_t wakeBuf[128] = { 0 };
    GetPrivateProfileStringW(L"Settings", L"assistant_wake_word", L"Свин", wakeBuf, 128, iniPath.c_str());
    g_config.assistantWakeWord = wakeBuf;
    if (g_config.assistantWakeWord.empty()) g_config.assistantWakeWord = L"Свин";

    g_config.enableProactiveAdvisor = GetPrivateProfileIntW(L"Settings", L"enable_proactive_advisor", 1, iniPath.c_str()) != 0;

    g_config.autoUpdate = GetPrivateProfileIntW(L"Settings", L"auto_update", 1, iniPath.c_str()) != 0;

    wchar_t verBuf[64] = { 0 };
    GetPrivateProfileStringW(L"Settings", L"app_version", L"2.4", verBuf, 64, iniPath.c_str());
    g_config.appVersion = WStringToUtf8(verBuf);

    wchar_t urlBuf[512] = { 0 };
    GetPrivateProfileStringW(L"Settings", L"update_url", L"https://raw.githubusercontent.com/MaksimGeraskin/WidgetBadPiggies/main/version.json", urlBuf, 512, iniPath.c_str());
    g_config.updateUrl = WStringToUtf8(urlBuf);

    ApplyAudioVolume();
}

void ConfigManager::Save() {
    std::wstring iniPath = GetExeDir() + L"\\config.ini";

    WritePrivateProfileStringW(L"Settings", L"pig_type", std::to_wstring(g_config.pigType).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"pig_size", std::to_wstring(g_config.pigSize).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"enable_sound", g_config.enableSound ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"sound_volume", std::to_wstring(g_config.soundVolume).c_str(), iniPath.c_str());

    WritePrivateProfileStringW(L"Settings", L"sound_kill", g_config.soundKill ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"sound_death", g_config.soundDeath ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"sound_low_hp", g_config.soundLowHp ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"sound_lotus", g_config.soundLotus ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"enable_voice_tts", g_config.enableVoiceTTS ? L"1" : L"0", iniPath.c_str());

    WritePrivateProfileStringW(L"Settings", L"enable_vignette", g_config.enableVignette ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"enable_beaten_pig", g_config.enableBeatenPig ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"enable_lotus_reminder", g_config.enableLotusReminder ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"lotus_mode", std::to_wstring(g_config.lotusMode).c_str(), iniPath.c_str());

    WritePrivateProfileStringW(L"Settings", L"hide_when_no_dota", g_config.hideWhenNoDota ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"pos_x", std::to_wstring(g_config.posX).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"pos_y", std::to_wstring(g_config.posY).c_str(), iniPath.c_str());

    WritePrivateProfileStringW(L"Settings", L"use_player_name", g_config.usePlayerName ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"player_name", g_config.playerName.c_str(), iniPath.c_str());

    WritePrivateProfileStringW(L"Settings", L"voice_gender", std::to_wstring(g_config.voiceGender).c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"enable_toxic_mode", g_config.enableToxicMode ? L"1" : L"0", iniPath.c_str());

    WritePrivateProfileStringW(L"Settings", L"enable_voice_assistant", g_config.enableVoiceAssistant ? L"1" : L"0", iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"assistant_wake_word", g_config.assistantWakeWord.c_str(), iniPath.c_str());
    WritePrivateProfileStringW(L"Settings", L"enable_proactive_advisor", g_config.enableProactiveAdvisor ? L"1" : L"0", iniPath.c_str());

    WritePrivateProfileStringW(L"Settings", L"auto_update", g_config.autoUpdate ? L"1" : L"0", iniPath.c_str());
    {
        std::wstring wVer = Utf8ToWString(g_config.appVersion);
        WritePrivateProfileStringW(L"Settings", L"app_version", wVer.c_str(), iniPath.c_str());
        std::wstring wUrl = Utf8ToWString(g_config.updateUrl);
        WritePrivateProfileStringW(L"Settings", L"update_url", wUrl.c_str(), iniPath.c_str());
    }
    
    SetAutoStart(g_config.autostart);
    ApplyAudioVolume();
}

bool ConfigManager::IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buffer[MAX_PATH] = { 0 };
        DWORD size = sizeof(buffer);
        DWORD type = 0;
        LSTATUS status = RegQueryValueExW(hKey, APP_NAME, NULL, &type, (LPBYTE)buffer, &size);
        RegCloseKey(hKey);
        return (status == ERROR_SUCCESS);
    }
    return false;
}

void ConfigManager::SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            std::wstring exe = L"\"" + GetExePath() + L"\"";
            RegSetValueExW(hKey, APP_NAME, 0, REG_SZ, (const BYTE*)exe.c_str(), (DWORD)((exe.length() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, APP_NAME);
        }
        RegCloseKey(hKey);
    }
}
