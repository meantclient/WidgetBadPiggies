#pragma once
#include "common.h"

struct AppConfig {
    int pigType = 1;              // 1 to 7
    int pigSize = 180;            // width/height in px
    bool enableSound = true;
    int soundVolume = 100;        // 0 to 100
    bool soundKill = true;
    bool soundDeath = true;
    bool soundLowHp = true;
    bool soundLotus = true;
    bool enableVoiceTTS = true;   // Voice acting for speech bubbles
    bool enableVignette = true;
    bool enableBeatenPig = true;
    bool enableLotusReminder = true;
    int lotusMode = 0;            // 0 = Auto (Normal 3m / Turbo 1.5m), 1 = Normal (3m), 2 = Turbo (1.5m)
    bool hideWhenNoDota = true;
    bool autostart = false;
    int posX = -1;                // -1 = default right corner
    int posY = -1;                // -1 = default bottom corner

    // Name personalization
    bool usePlayerName = true;
    std::wstring playerName = L"Марат";

    // Voice & Personality
    int voiceGender = 1;          // 0 = Female (Приятный женский), 1 = Male (Павел)
    bool enableToxicMode = false; // "Говорить наравне с человеком" (мощная прожарка / токсик)

    // Voice Assistant & Hero Advisor
    bool enableVoiceAssistant = true;
    std::wstring assistantWakeWord = L"Свин";
    bool enableProactiveAdvisor = true;

    // Dynamic Island & Visual Customization
    bool enablePigShadow = true;
    bool enableDynamicIsland = false;

    // Microphone device selection
    std::wstring microphoneDevice = L""; // Empty = default system microphone

    // Auto-update
    bool autoUpdate = true;
    std::string appVersion = "5.0.1";
    std::string updateUrl = "https://raw.githubusercontent.com/meantclient/WidgetBadPiggies/main/version.json";
};

class ConfigManager {
public:
    static AppConfig& Get();
    static void Load();
    static void Save();
    static void SetAutoStart(bool enable);
    static bool IsAutoStartEnabled();
    static std::wstring GetExePath();
    static std::wstring GetExeDir();
    static std::wstring GetAssetPath(const std::wstring& filename);
    static void ApplyAudioVolume();
};
