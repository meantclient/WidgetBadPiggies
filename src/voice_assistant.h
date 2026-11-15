#pragma once
#include "common.h"
#include <string>
#include <atomic>
#include <thread>
#include <sapi.h>

class VoiceAssistant {
public:
    static VoiceAssistant& Instance();

    bool Start();
    void Stop();

    // Enumerate available microphone devices for settings UI
    static std::vector<std::pair<std::wstring, std::wstring>> GetAvailableMicrophones();

    // Trigger advice programmatically (e.g. from hotkey or UI button)
    void AskAdvice(int queryType = 0); // 0 = Buy, 1 = Skill, 2 = General

    bool IsRunning() const { return running_.load(); }
    bool IsAwake() const;

private:
    VoiceAssistant();
    ~VoiceAssistant();

    void ListeningThread();
    void ProcessRecognizedText(const std::wstring& text);

    std::atomic<bool> running_{ false };
    std::thread listenThread_;
    std::chrono::steady_clock::time_point awakeUntil_{};

    ISpRecoContext* pRecoCtx_ = NULL;
    ISpRecoGrammar* pGrammar_ = NULL;
    ISpRecognizer* pInprocRec_ = NULL;
    ISpAudio* pAudio_ = NULL;
};
