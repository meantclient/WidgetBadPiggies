#pragma once
#include "common.h"
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <sapi.h>

class VoiceAssistant {
public:
    static VoiceAssistant& Instance();

    bool Start();
    void Stop();

    // Enumerate available microphone devices for settings UI
    static std::vector<std::pair<std::wstring, std::wstring>> GetAvailableMicrophones();

    // Trigger advice programmatically (e.g. from hotkey or UI button)
    void AskAdvice(int queryType = 0); // 0 = Buy, 1 = Skill, 2 = General, 3 = Random Meme
    void TriggerMeme(const std::wstring& memeType = L"");

    bool IsRunning() const { return running_.load(); }
    bool IsAwake() const;
    bool IsUsingVosk() const { return usingVosk_.load(); }

    void ProcessRecognizedText(const std::wstring& text, bool isFinal = true);

private:
    VoiceAssistant();
    ~VoiceAssistant();

    void SapiListeningThread();

    std::atomic<bool> running_{ false };
    std::atomic<bool> usingVosk_{ false };
    std::thread sapiThread_;
    std::chrono::steady_clock::time_point awakeUntil_{};

    // SAPI fallback
    ISpRecoContext* pRecoCtx_ = NULL;
    ISpRecoGrammar* pGrammar_ = NULL;
    ISpRecognizer* pInprocRec_ = NULL;
    ISpAudio* pAudio_ = NULL;
};
