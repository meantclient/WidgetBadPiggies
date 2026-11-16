#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

class VoskEngine {
public:
    static VoskEngine& Instance();

    bool Init(const std::wstring& customModelPath = L"");
    bool StartListening(std::function<void(const std::wstring& text, bool isFinal)> callback);
    void StopListening();

    bool IsModelLoaded() const { return modelLoaded_; }
    bool IsListening() const { return isListening_.load(); }
    void ResetRecognizer();
    void SetConversationMode(bool active);

    static std::vector<std::pair<std::wstring, UINT>> GetInputDevices();
    void SetInputDevice(const std::wstring& deviceName);

    std::wstring GetStatusString() const;

private:
    VoskEngine();
    ~VoskEngine();

    bool LoadVoskLibrary();
    void CaptureThread();
    std::wstring ResolveModelPath(const std::wstring& customPath);

    HMODULE hVoskDll_ = NULL;
    void* pModel_ = nullptr;
    void* pRecognizer_ = nullptr;
    bool modelLoaded_ = false;

    std::atomic<bool> isListening_{ false };
    std::thread workerThread_;
    std::function<void(const std::wstring&, bool)> speechCallback_;
    std::recursive_mutex recognizerMutex_;

    std::wstring selectedDeviceName_ = L"";
    UINT selectedDeviceId_ = WAVE_MAPPER;

    // Vosk C Function Pointers
    typedef void* (*FnVoskModelNew)(const char* path);
    typedef void (*FnVoskModelFree)(void* model);
    typedef void* (*FnVoskRecognizerNew)(void* model, float sample_rate);
    typedef void* (*FnVoskRecognizerNewGrammar)(void* model, float sample_rate, const char* grammar);
    typedef void (*FnVoskRecognizerFree)(void* recognizer);
    typedef int (*FnVoskRecognizerAcceptWaveform)(void* recognizer, const char* data, int length);
    typedef const char* (*FnVoskRecognizerResult)(void* recognizer);
    typedef const char* (*FnVoskRecognizerPartialResult)(void* recognizer);
    typedef const char* (*FnVoskRecognizerFinalResult)(void* recognizer);
    typedef void (*FnVoskRecognizerReset)(void* recognizer);
    typedef void (*FnVoskSetLogLevel)(int level);

    FnVoskModelNew fnVoskModelNew_ = nullptr;
    FnVoskModelFree fnVoskModelFree_ = nullptr;
    FnVoskRecognizerNew fnVoskRecognizerNew_ = nullptr;
    FnVoskRecognizerNewGrammar fnVoskRecognizerNewGrammar_ = nullptr;
    FnVoskRecognizerFree fnVoskRecognizerFree_ = nullptr;
    FnVoskRecognizerAcceptWaveform fnVoskRecognizerAcceptWaveform_ = nullptr;
    FnVoskRecognizerResult fnVoskRecognizerResult_ = nullptr;
    FnVoskRecognizerPartialResult fnVoskRecognizerPartialResult_ = nullptr;
    FnVoskRecognizerFinalResult fnVoskRecognizerFinalResult_ = nullptr;
    FnVoskRecognizerReset fnVoskRecognizerReset_ = nullptr;
    FnVoskSetLogLevel fnVoskSetLogLevel_ = nullptr;
    bool conversationMode_ = true;
};
