#pragma once
#include <string>
#include <vector>
#include <map>
#include <windows.h>

struct DialogHistoryItem {
    std::wstring userQuery;
    std::wstring pigResponse;
    DWORD timestamp;
};

class AiMemory {
public:
    static AiMemory& Instance();

    void Load();
    void Save();

    // Universal Fact Memory (Key-Value & general remembered notes)
    void SetFact(const std::wstring& key, const std::wstring& value);
    std::wstring GetFact(const std::wstring& key) const;
    std::wstring FindMatchingFact(const std::wstring& query) const;
    std::map<std::wstring, std::wstring> GetAllFacts() const;
    void ClearMemory();
    std::wstring GetMemorySummary() const;

    // Optional preferred music service (if user ever configured one)
    void SetMusicService(const std::wstring& service, const std::wstring& customUrl = L"");
    std::wstring GetMusicService() const;
    std::wstring GetMusicServiceNameRu() const;
    std::wstring GetCustomPlaylistUrl() const;

    // Dialog context memory
    void AddDialogHistory(const std::wstring& userQuery, const std::wstring& pigResponse);
    std::vector<DialogHistoryItem> GetRecentHistory(int maxCount = 5) const;
    std::wstring GetLastTopic() const;
    void ObserveStyle(const std::wstring& text);
    bool PrefersConcise() const;
    bool PrefersCasual() const;

private:
    AiMemory();
    ~AiMemory();

    std::wstring musicService_ = L"yandex";
    std::wstring customPlaylistUrl_ = L"";
    std::map<std::wstring, std::wstring> facts_;
    std::vector<DialogHistoryItem> history_;
    std::wstring lastTopic_ = L"";
    int casualStyle_ = 0;
    int conciseStyle_ = 0;
};

class AiBrain {
public:
    static AiBrain& Instance();

    void Init();

    // Main AI decision engine: processes user natural language and returns an answer + executes actions
    bool ProcessSpeech(const std::wstring& userSpeech, std::wstring& outReply);

    // Automation Actions
    bool OpenMusicPlaylist(const std::wstring& serviceOverride = L"");
    void ControlMedia(int action); // 0: Play/Pause, 1: Next, 2: Prev, 3: Vol+, 4: Vol-, 5: Mute
    void LaunchApp(const std::wstring& appName);
    void SearchWeb(const std::wstring& query);
    void MinimizeAllWindows();
    void TakeScreenshot();

    // Utilities
    std::wstring GetCurrentTimeRu();
    std::wstring GetCurrentDateRu();

    // Dynamic Conversational AI Engine
    std::wstring GenerateConversationalReply(const std::wstring& text);

private:
    AiBrain();
    ~AiBrain();

};
