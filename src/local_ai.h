#pragma once
#include <string>
#include <windows.h>

class LocalAiEngine {
public:
    static LocalAiEngine& Instance();
    bool Generate(const std::wstring& systemPrompt, const std::wstring& userPrompt, std::wstring& reply);
    bool IsModelInstalled() const;
    void Shutdown();

private:
    LocalAiEngine() = default;
    ~LocalAiEngine();
    bool EnsureModel(std::wstring& modelPath);
    bool EnsureServer(const std::wstring& modelPath);
    bool IsServerReady() const;
    bool SendChatRequest(const std::wstring& systemPrompt, const std::wstring& userPrompt, std::wstring& reply);

    HANDLE process_ = nullptr;
    HANDLE processThread_ = nullptr;
    HANDLE job_ = nullptr;
    bool processInJob_ = false;
    unsigned short port_ = 0;
    CRITICAL_SECTION lock_{};
    bool lockInitialized_ = false;
};
