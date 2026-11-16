#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "vosk_engine.h"
#include "voice.h"
#include "config.h"
#include "nlohmann/json.hpp"
#include <shlwapi.h>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")

VoskEngine& VoskEngine::Instance() {
    static VoskEngine instance;
    return instance;
}

VoskEngine::VoskEngine() {}

VoskEngine::~VoskEngine() {
    StopListening();
    if (pRecognizer_ && fnVoskRecognizerFree_) {
        fnVoskRecognizerFree_(pRecognizer_);
        pRecognizer_ = nullptr;
    }
    if (pModel_ && fnVoskModelFree_) {
        fnVoskModelFree_(pModel_);
        pModel_ = nullptr;
    }
    if (hVoskDll_) {
        FreeLibrary(hVoskDll_);
        hVoskDll_ = NULL;
    }
}

bool VoskEngine::LoadVoskLibrary() {
    if (hVoskDll_) return true;

    // Search in current directory, exe directory, or system
    wchar_t exePath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    PathRemoveFileSpecW(exePath);

    std::wstring dllPath = std::wstring(exePath) + L"\\libvosk.dll";
    hVoskDll_ = LoadLibraryW(dllPath.c_str());
    if (!hVoskDll_) {
        hVoskDll_ = LoadLibraryW(L"libvosk.dll");
    }
    if (!hVoskDll_) {
        return false;
    }

    fnVoskModelNew_ = (FnVoskModelNew)GetProcAddress(hVoskDll_, "vosk_model_new");
    fnVoskModelFree_ = (FnVoskModelFree)GetProcAddress(hVoskDll_, "vosk_model_free");
    fnVoskRecognizerNew_ = (FnVoskRecognizerNew)GetProcAddress(hVoskDll_, "vosk_recognizer_new");
    fnVoskRecognizerNewGrammar_ = (FnVoskRecognizerNewGrammar)GetProcAddress(hVoskDll_, "vosk_recognizer_new_grm");
    fnVoskRecognizerFree_ = (FnVoskRecognizerFree)GetProcAddress(hVoskDll_, "vosk_recognizer_free");
    fnVoskRecognizerAcceptWaveform_ = (FnVoskRecognizerAcceptWaveform)GetProcAddress(hVoskDll_, "vosk_recognizer_accept_waveform");
    fnVoskRecognizerResult_ = (FnVoskRecognizerResult)GetProcAddress(hVoskDll_, "vosk_recognizer_result");
    fnVoskRecognizerPartialResult_ = (FnVoskRecognizerPartialResult)GetProcAddress(hVoskDll_, "vosk_recognizer_partial_result");
    fnVoskRecognizerFinalResult_ = (FnVoskRecognizerFinalResult)GetProcAddress(hVoskDll_, "vosk_recognizer_final_result");
    fnVoskRecognizerReset_ = (FnVoskRecognizerReset)GetProcAddress(hVoskDll_, "vosk_recognizer_reset");
    fnVoskSetLogLevel_ = (FnVoskSetLogLevel)GetProcAddress(hVoskDll_, "vosk_set_log_level");

    if (!fnVoskModelNew_ || !fnVoskRecognizerNew_ || !fnVoskRecognizerAcceptWaveform_) {
        FreeLibrary(hVoskDll_);
        hVoskDll_ = NULL;
        return false;
    }

    if (fnVoskSetLogLevel_) {
        fnVoskSetLogLevel_(-1); // Suppress verbose internal logging
    }

    return true;
}

std::wstring VoskEngine::ResolveModelPath(const std::wstring& customPath) {
    std::vector<std::wstring> candidates;
    if (!customPath.empty()) {
        candidates.push_back(customPath);
    }

    wchar_t exeDir[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exeDir, MAX_PATH);
    PathRemoveFileSpecW(exeDir);

    candidates.push_back(std::wstring(exeDir) + L"\\model");
    candidates.push_back(L".\\model");
    candidates.push_back(L"model");

    // Check user profile cache directory
    wchar_t userProfile[MAX_PATH] = { 0 };
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        candidates.push_back(std::wstring(userProfile) + L"\\.cache\\vosk\\vosk-model-small-ru-0.22");
    }

    for (const auto& candidate : candidates) {
        DWORD attrs = GetFileAttributesW(candidate.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Check for am/final.mdl
            std::wstring amPath = candidate + L"\\am\\final.mdl";
            if (PathFileExistsW(amPath.c_str())) {
                return candidate;
            }
        }
    }

    return L"";
}

bool VoskEngine::Init(const std::wstring& customModelPath) {
    if (modelLoaded_ && pModel_) return true;

    if (!LoadVoskLibrary()) {
        return false;
    }

    std::wstring foundPath = ResolveModelPath(customModelPath);
    if (foundPath.empty()) {
        return false;
    }

    // Convert to 8.3 Short Path to prevent any non-ASCII / Cyrillic path issues with Kaldi C runtime
    wchar_t shortPath[MAX_PATH] = { 0 };
    DWORD spLen = GetShortPathNameW(foundPath.c_str(), shortPath, MAX_PATH);
    std::string pathAnsi;
    if (spLen > 0 && spLen < MAX_PATH) {
        int bufSize = WideCharToMultiByte(CP_ACP, 0, shortPath, -1, NULL, 0, NULL, NULL);
        pathAnsi.resize(bufSize - 1);
        WideCharToMultiByte(CP_ACP, 0, shortPath, -1, &pathAnsi[0], bufSize, NULL, NULL);
    } else {
        int bufSize = WideCharToMultiByte(CP_UTF8, 0, foundPath.c_str(), -1, NULL, 0, NULL, NULL);
        pathAnsi.resize(bufSize - 1);
        WideCharToMultiByte(CP_UTF8, 0, foundPath.c_str(), -1, &pathAnsi[0], bufSize, NULL, NULL);
    }

    pModel_ = fnVoskModelNew_(pathAnsi.c_str());
    if (!pModel_) {
        return false;
    }

    conversationMode_ = false;
    if (fnVoskRecognizerNewGrammar_) {
        std::wstring wake = ConfigManager::Get().assistantWakeWord;
        if (wake.empty()) wake = L"Свин";
        int n = WideCharToMultiByte(CP_UTF8, 0, wake.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(n > 0 ? n : 0, '\0');
        if (n > 0) { WideCharToMultiByte(CP_UTF8, 0, wake.c_str(), -1, &utf8[0], n, nullptr, nullptr); utf8.pop_back(); }
        std::string grammar = "[\"" + utf8 + "\"]";
        pRecognizer_ = fnVoskRecognizerNewGrammar_(pModel_, 16000.0f, grammar.c_str());
    }
    if (!pRecognizer_) {
        conversationMode_ = true;
        pRecognizer_ = fnVoskRecognizerNew_(pModel_, 16000.0f);
    }
    if (!pRecognizer_) {
        fnVoskModelFree_(pModel_);
        pModel_ = nullptr;
        return false;
    }

    modelLoaded_ = true;
    return true;
}

std::vector<std::pair<std::wstring, UINT>> VoskEngine::GetInputDevices() {
    std::vector<std::pair<std::wstring, UINT>> devices;
    devices.push_back({ L"По умолчанию (Системный)", WAVE_MAPPER });

    UINT numDevs = waveInGetNumDevs();
    for (UINT i = 0; i < numDevs; ++i) {
        WAVEINCAPSW caps = { 0 };
        if (waveInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
            devices.push_back({ std::wstring(caps.szPname), i });
        }
    }
    return devices;
}

void VoskEngine::SetInputDevice(const std::wstring& deviceName) {
    selectedDeviceName_ = deviceName;
    selectedDeviceId_ = WAVE_MAPPER;

    if (!deviceName.empty()) {
        auto devList = GetInputDevices();
        for (const auto& dev : devList) {
            if (dev.first.find(deviceName) != std::wstring::npos || deviceName.find(dev.first) != std::wstring::npos) {
                selectedDeviceId_ = dev.second;
                break;
            }
        }
    }
}

bool VoskEngine::StartListening(std::function<void(const std::wstring& text, bool isFinal)> callback) {
    if (isListening_.load()) return true;
    if (!modelLoaded_ && !Init()) return false;

    speechCallback_ = callback;
    isListening_.store(true);
    workerThread_ = std::thread(&VoskEngine::CaptureThread, this);
    return true;
}

void VoskEngine::StopListening() {
    if (!isListening_.load()) return;
    isListening_.store(false);

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

void VoskEngine::ResetRecognizer() {
    std::lock_guard<std::recursive_mutex> lock(recognizerMutex_);
    if (pRecognizer_ && fnVoskRecognizerReset_) {
        fnVoskRecognizerReset_(pRecognizer_);
    }
}

void VoskEngine::SetConversationMode(bool active) {
    std::lock_guard<std::recursive_mutex> lock(recognizerMutex_);
    if (!pModel_ || !pRecognizer_ || conversationMode_ == active) return;
    void* replacement = nullptr;
    if (!active && fnVoskRecognizerNewGrammar_) {
        std::wstring wake = ConfigManager::Get().assistantWakeWord;
        if (wake.empty()) wake = L"Свин";
        int n = WideCharToMultiByte(CP_UTF8, 0, wake.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(n > 0 ? n : 0, '\0');
        if (n > 0) { WideCharToMultiByte(CP_UTF8, 0, wake.c_str(), -1, &utf8[0], n, nullptr, nullptr); utf8.pop_back(); }
        std::string grammar = "[\"" + utf8 + "\"]";
        replacement = fnVoskRecognizerNewGrammar_(pModel_, 16000.0f, grammar.c_str());
    } else if (active) {
        replacement = fnVoskRecognizerNew_(pModel_, 16000.0f);
    }
    if (!replacement) return;
    if (fnVoskRecognizerFree_) fnVoskRecognizerFree_(pRecognizer_);
    pRecognizer_ = replacement;
    conversationMode_ = active;
}

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    if (size <= 1) return L"";
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

void VoskEngine::CaptureThread() {
    WAVEFORMATEX wfx = { 0 };
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 16000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample / 8); // 2 bytes
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign; // 32000 bytes/sec
    wfx.cbSize = 0;

    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    HWAVEIN hWaveIn = NULL;

    MMRESULT mr = waveInOpen(&hWaveIn, selectedDeviceId_, &wfx, (DWORD_PTR)hEvent, 0, CALLBACK_EVENT);
    if (mr != MMSYSERR_NOERROR) {
        // Fallback to default wave mapper if selected device failed
        mr = waveInOpen(&hWaveIn, WAVE_MAPPER, &wfx, (DWORD_PTR)hEvent, 0, CALLBACK_EVENT);
    }

    if (mr != MMSYSERR_NOERROR || !hWaveIn) {
        if (hEvent) CloseHandle(hEvent);
        isListening_.store(false);
        return;
    }

    const int NUM_BUFFERS = 4;
    const int BUFFER_SIZE = 3200; // 100ms at 16kHz 16-bit mono
    WAVEHDR headers[NUM_BUFFERS];
    std::vector<std::vector<char>> buffers(NUM_BUFFERS, std::vector<char>(BUFFER_SIZE));

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        memset(&headers[i], 0, sizeof(WAVEHDR));
        headers[i].lpData = buffers[i].data();
        headers[i].dwBufferLength = BUFFER_SIZE;
        waveInPrepareHeader(hWaveIn, &headers[i], sizeof(WAVEHDR));
        waveInAddBuffer(hWaveIn, &headers[i], sizeof(WAVEHDR));
    }

    waveInStart(hWaveIn);

    while (isListening_.load()) {
        DWORD waitRes = WaitForSingleObject(hEvent, 100);
        if (!isListening_.load()) break;

        if (waitRes == WAIT_OBJECT_0) {
            for (int i = 0; i < NUM_BUFFERS; ++i) {
                if (headers[i].dwFlags & WHDR_DONE) {
                    if (headers[i].dwBytesRecorded > 0) {
                        // Software microphone gain boost (3.5x) so normal and quiet speech is easily decoded by Kaldi
                        short* pSamples = (short*)headers[i].lpData;
                        int sCount = headers[i].dwBytesRecorded / 2;
                        int maxAmp = 0;
                        for (int s = 0; s < sCount; ++s) {
                            int val = (int)(pSamples[s] * 3.5f);
                            if (val > 32767) val = 32767;
                            else if (val < -32768) val = -32768;
                            pSamples[s] = (short)val;
                            int absVal = abs(val);
                            if (absVal > maxAmp) maxAmp = absVal;
                        }

                        // Periodic debug log for audio levels every 3 seconds
                        static DWORD s_lastAudioLog = 0;
                        DWORD nowTick = GetTickCount();
                        if (nowTick - s_lastAudioLog >= 3000) {
                            s_lastAudioLog = nowTick;
                            FILE* fLog = _wfopen(L"voice_debug.log", L"a, ccs=UTF-8");
                            if (fLog) {
                                fwprintf(fLog, L"[MIC LEVEL] Peak amplitude: %d / 32768\n", maxAmp);
                                fclose(fLog);
                            }
                        }

                        // Drop audio packet if TTS is currently speaking so the pig never reacts to itself!
                        if (!VoiceService::Instance().IsSpeaking()) {
                            std::wstring callbackText = L"";
                            bool callbackFinal = false;
                            bool haveCallback = false;

                            {
                                std::lock_guard<std::recursive_mutex> lock(recognizerMutex_);
                                if (pRecognizer_ && fnVoskRecognizerAcceptWaveform_) {
                                    int accepted = fnVoskRecognizerAcceptWaveform_(pRecognizer_, headers[i].lpData, headers[i].dwBytesRecorded);
                                    if (accepted) {
                                        const char* resJson = fnVoskRecognizerResult_(pRecognizer_);
                                        if (resJson) {
                                            try {
                                                auto j = nlohmann::json::parse(resJson);
                                                if (j.contains("text")) {
                                                    std::string textUtf8 = j["text"].get<std::string>();
                                                    if (!textUtf8.empty()) {
                                                        callbackText = Utf8ToWide(textUtf8);
                                                        callbackFinal = true;
                                                        haveCallback = true;
                                                    }
                                                }
                                            } catch (...) {}
                                        }
                                    } else {
                                        const char* partJson = fnVoskRecognizerPartialResult_(pRecognizer_);
                                        if (partJson) {
                                            try {
                                                auto j = nlohmann::json::parse(partJson);
                                                if (j.contains("partial")) {
                                                    std::string partialUtf8 = j["partial"].get<std::string>();
                                                    if (!partialUtf8.empty()) {
                                                        callbackText = Utf8ToWide(partialUtf8);
                                                        callbackFinal = false;
                                                        haveCallback = true;
                                                    }
                                                }
                                            } catch (...) {}
                                        }
                                    }
                                }
                            } // recognizerMutex_ released here BEFORE invoking callbacks

                            if (haveCallback && speechCallback_) {
                                if (callbackFinal) {
                                    FILE* fLog = _wfopen(L"voice_debug.log", L"a, ccs=UTF-8");
                                    if (fLog) {
                                        fwprintf(fLog, L"[VOSK FINAL] '%s'\n", callbackText.c_str());
                                        fclose(fLog);
                                    }
                                }
                                speechCallback_(callbackText, callbackFinal);
                            }
                        }
                    }

                    // Requeue buffer for continuous streaming
                    headers[i].dwFlags &= ~WHDR_DONE;
                    waveInAddBuffer(hWaveIn, &headers[i], sizeof(WAVEHDR));
                }
            }
        }
    }

    waveInStop(hWaveIn);
    waveInReset(hWaveIn);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        waveInUnprepareHeader(hWaveIn, &headers[i], sizeof(WAVEHDR));
    }
    waveInClose(hWaveIn);
    CloseHandle(hEvent);
}

std::wstring VoskEngine::GetStatusString() const {
    if (!hVoskDll_) return L"Vosk DLL не найдена (используется SAPI)";
    if (!modelLoaded_) return L"Модель Vosk не загружена (используется SAPI)";
    if (isListening_.load()) return L"🟢 Vosk Офлайн AI: Слушает микрофон";
    return L"⚪ Vosk Офлайн AI: Готов к работе";
}
