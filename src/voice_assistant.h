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

    // Trigger advice programmatically (e.g. from hotkey or UI button)
    void AskAdvice(int queryType = 0); // 0 = Buy, 1 = Skill, 2 = General

    bool IsRunning() const { return running_.load(); }

private:
    VoiceAssistant();
    ~VoiceAssistant();

    void ListeningThread();
    void ProcessRecognizedText(const std::wstring& text);

    std::atomic<bool> running_{ false };
    std::thread listenThread_;

    ISpRecoContext* pRecoCtx_ = NULL;
    ISpRecoGrammar* pGrammar_ = NULL;
    ISpRecognizer* pInprocRec_ = NULL;
};
