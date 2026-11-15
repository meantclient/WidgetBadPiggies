#pragma once
#include "common.h"
#include <sapi.h>

class VoiceService {
public:
    static VoiceService& Instance();

    void Init();
    void ApplyVoiceSettings();
    void Speak(const std::wstring& text);
    bool IsSpeaking() const;
    void Stop();

private:
    VoiceService();
    ~VoiceService();

    ISpVoice* pVoice_ = nullptr;
    bool initialized_ = false;
    int currentGender_ = -1;
};
