#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "voice_assistant.h"
#include "vosk_engine.h"
#include "config.h"
#include "hero_advisor.h"
#include "overlay.h"
#include "voice.h"
#include "settings_gui.h"
#include "ai_brain.h"
#include <algorithm>
#include <vector>
#include <chrono>

#pragma comment(lib, "ole32.lib")

#define WM_VOICE_TRIGGER (WM_USER + 201)

extern HWND g_hMsgWnd;

static bool HasWord(const std::wstring& text, const std::wstring& word) {
    size_t pos = 0;
    while ((pos = text.find(word, pos)) != std::wstring::npos) {
        bool leftOk = (pos == 0 || !iswalnum(text[pos - 1]));
        bool rightOk = (pos + word.length() >= text.length() || !iswalnum(text[pos + word.length()]));
        if (leftOk && rightOk) return true;
        pos += word.length();
    }
    return false;
}

static void DispatchVoiceAdvice(const std::wstring& advice) {
    FILE* fLog = _wfopen(L"voice_debug.log", L"a, ccs=UTF-8");
    if (fLog) {
        fwprintf(fLog, L"[DISPATCH] Advice: '%s' (g_hMsgWnd=%p)\n", advice.c_str(), g_hMsgWnd);
        fclose(fLog);
    }

    // Update settings GUI in real time with the pig's answer
    SettingsWindow::Instance().SetLastAnswerText(advice);

    if (g_hMsgWnd) {
        std::wstring* pCopy = new std::wstring(advice);
        PostMessageW(g_hMsgWnd, WM_VOICE_TRIGGER, 0, (LPARAM)pCopy);
    } else {
        OverlayWindow::Instance().TriggerAdvice(advice);
    }
}

VoiceAssistant& VoiceAssistant::Instance() {
    static VoiceAssistant instance;
    return instance;
}

VoiceAssistant::VoiceAssistant() {}

VoiceAssistant::~VoiceAssistant() {
    Stop();
}

bool VoiceAssistant::Start() {
    if (running_.load()) return true;
    if (!ConfigManager::Get().enableVoiceAssistant) return false;

    running_.store(true);

    // 1. Primary Engine: Vosk Speech Recognition (Offline Kaldi Russian Model)
    VoskEngine::Instance().SetInputDevice(ConfigManager::Get().microphoneDevice);
    bool voskOk = VoskEngine::Instance().StartListening([this](const std::wstring& text, bool isFinal) {
        if (!text.empty()) {
            this->ProcessRecognizedText(text, isFinal);
        }
    });

    if (voskOk) {
        usingVosk_.store(true);
        return true;
    }

    // 2. Fallback Engine: Windows SAPI Inproc/Shared Recognizer
    usingVosk_.store(false);
    sapiThread_ = std::thread(&VoiceAssistant::SapiListeningThread, this);
    return true;
}

void VoiceAssistant::Stop() {
    if (!running_.load()) return;
    running_.store(false);

    if (usingVosk_.load()) {
        VoskEngine::Instance().StopListening();
        usingVosk_.store(false);
    }

    if (pAudio_) {
        pAudio_->SetState(SPAS_STOP, 0);
        pAudio_->Release();
        pAudio_ = NULL;
    }
    if (pGrammar_) {
        pGrammar_->SetGrammarState(SPGS_DISABLED);
        pGrammar_->Release();
        pGrammar_ = NULL;
    }
    if (pRecoCtx_) {
        pRecoCtx_->Release();
        pRecoCtx_ = NULL;
    }
    if (pInprocRec_) {
        pInprocRec_->Release();
        pInprocRec_ = NULL;
    }

    if (sapiThread_.joinable()) {
        sapiThread_.join();
    }
}

bool VoiceAssistant::IsAwake() const {
    return (std::chrono::steady_clock::now() < awakeUntil_);
}

void VoiceAssistant::NotifyStatus(const std::wstring& text) {
    DispatchVoiceAdvice(text);
}

std::vector<std::pair<std::wstring, std::wstring>> VoiceAssistant::GetAvailableMicrophones() {
    std::vector<std::pair<std::wstring, std::wstring>> result;
    auto devList = VoskEngine::GetInputDevices();
    for (const auto& dev : devList) {
        result.push_back({ dev.first, dev.first });
    }
    return result;
}

void VoiceAssistant::SapiListeningThread() {
    CoInitialize(NULL);

    HRESULT hr = CoCreateInstance(CLSID_SpInprocRecognizer, NULL, CLSCTX_ALL, IID_ISpRecognizer, (void**)&pInprocRec_);
    if (SUCCEEDED(hr) && pInprocRec_) {
        bool audioSet = false;
        std::wstring targetMic = ConfigManager::Get().microphoneDevice;

        ISpObjectTokenCategory* pCat = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL, IID_ISpObjectTokenCategory, (void**)&pCat)) && pCat) {
            if (SUCCEEDED(pCat->SetId(SPCAT_AUDIOIN, FALSE))) {
                if (!targetMic.empty()) {
                    IEnumSpObjectTokens* pEnum = nullptr;
                    if (SUCCEEDED(pCat->EnumTokens(NULL, NULL, &pEnum)) && pEnum) {
                        ISpObjectToken* pToken = nullptr;
                        while (pEnum->Next(1, &pToken, NULL) == S_OK) {
                            WCHAR* desc = nullptr;
                            pToken->GetStringValue(NULL, &desc);
                            WCHAR* pId = nullptr;
                            pToken->GetId(&pId);
                            if ((desc && targetMic == desc) || (pId && targetMic == pId)) {
                                if (SUCCEEDED(pInprocRec_->SetInput(pToken, TRUE))) {
                                    audioSet = true;
                                }
                            }
                            if (desc) CoTaskMemFree(desc);
                            if (pId) CoTaskMemFree(pId);
                            pToken->Release();
                            if (audioSet) break;
                        }
                        pEnum->Release();
                    }
                }

                if (!audioSet) {
                    WCHAR* defaultId = nullptr;
                    if (SUCCEEDED(pCat->GetDefaultTokenId(&defaultId)) && defaultId) {
                        ISpObjectToken* pToken = nullptr;
                        if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectToken, NULL, CLSCTX_ALL, IID_ISpObjectToken, (void**)&pToken)) && pToken) {
                            if (SUCCEEDED(pToken->SetId(NULL, defaultId, FALSE))) {
                                if (SUCCEEDED(pInprocRec_->SetInput(pToken, TRUE))) {
                                    audioSet = true;
                                }
                            }
                            pToken->Release();
                        }
                        CoTaskMemFree(defaultId);
                    }
                }
            }
            pCat->Release();
        }

        if (!audioSet) {
            HRESULT hrAudio = CoCreateInstance(CLSID_SpMMAudioIn, NULL, CLSCTX_ALL, IID_ISpAudio, (void**)&pAudio_);
            if (SUCCEEDED(hrAudio) && pAudio_) {
                pAudio_->SetState(SPAS_RUN, 0);
                pInprocRec_->SetInput(pAudio_, TRUE);
            }
        }
        pInprocRec_->CreateRecoContext(&pRecoCtx_);
    }

    if (!pRecoCtx_) {
        hr = CoCreateInstance(CLSID_SpSharedRecoContext, NULL, CLSCTX_ALL, IID_ISpRecoContext, (void**)&pRecoCtx_);
    }

    if (!pRecoCtx_) {
        CoUninitialize();
        return;
    }

    hr = pRecoCtx_->CreateGrammar(1, &pGrammar_);
    if (FAILED(hr) || !pGrammar_) {
        pRecoCtx_->Release();
        pRecoCtx_ = NULL;
        CoUninitialize();
        return;
    }

    pGrammar_->LoadDictation(NULL, SPLO_STATIC);
    pGrammar_->SetDictationState(SPRS_ACTIVE);

    pRecoCtx_->SetInterest(
        SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS),
        SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS)
    );
    pRecoCtx_->Resume(0);

    HANDLE hEvent = pRecoCtx_->GetNotifyEventHandle();

    while (running_.load()) {
        if (hEvent != NULL) {
            DWORD waitRes = WaitForSingleObject(hEvent, 150);
            if (waitRes == WAIT_OBJECT_0) {
                SPEVENT spEvent;
                ULONG fetched = 0;
                while (pRecoCtx_->GetEvents(1, &spEvent, &fetched) == S_OK && fetched > 0) {
                    if (spEvent.eEventId == SPEI_RECOGNITION || spEvent.eEventId == SPEI_FALSE_RECOGNITION) {
                        if (VoiceService::Instance().IsSpeaking()) {
                            continue;
                        }
                        ISpRecoResult* pResult = reinterpret_cast<ISpRecoResult*>(spEvent.lParam);
                        if (pResult) {
                            WCHAR* pwszText = NULL;
                            if (SUCCEEDED(pResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &pwszText, NULL))) {
                                if (pwszText && wcslen(pwszText) > 0) {
                                    ProcessRecognizedText(pwszText, true);
                                    CoTaskMemFree(pwszText);
                                }
                            }
                        }
                    }
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    }

    if (pGrammar_) {
        pGrammar_->SetGrammarState(SPGS_DISABLED);
        pGrammar_->Release();
        pGrammar_ = NULL;
    }
    if (pRecoCtx_) {
        pRecoCtx_->Release();
        pRecoCtx_ = NULL;
    }
    if (pInprocRec_) {
        pInprocRec_->Release();
        pInprocRec_ = NULL;
    }

    CoUninitialize();
}

void VoiceAssistant::ProcessRecognizedText(const std::wstring& rawText, bool isFinal) {
    FILE* fLog = _wfopen(L"voice_debug.log", L"a, ccs=UTF-8");
    if (fLog) {
        fwprintf(fLog, L"[RECOG] isFinal=%d text='%s'\n", isFinal, rawText.c_str());
        fclose(fLog);
    }

    std::wstring text = rawText;
    for (auto& ch : text) ch = (wchar_t)towlower(ch);

    // Update real-time status in Settings window so user sees microphone is hearing live
    SettingsWindow::Instance().SetLastRecognizedText(rawText);

    // CRITICAL: Only trigger actions on final recognition hypotheses!
    // Partial hypotheses update the Settings UI status only so the pig never cuts the user off mid-sentence.
    if (!isFinal) {
        if (usingVosk_.load() && std::chrono::steady_clock::now() >= awakeUntil_) {
            VoskEngine::Instance().SetConversationMode(false);
        }
        return;
    }

    static DWORD s_lastTriggerTick = 0;
    DWORD nowTick = GetTickCount();

    // Debounce triggers: at least 800 ms between distinct voice responses
    if (nowTick - s_lastTriggerTick < 800) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    bool isAwakeWindow = (now < awakeUntil_);

    std::wstring wakeWord = ConfigManager::Get().assistantWakeWord;
    for (auto& ch : wakeWord) ch = (wchar_t)towlower(ch);

    bool hasWakeWord = (!wakeWord.empty() && text.find(wakeWord) != std::wstring::npos) ||
        text.find(L"свин") != std::wstring::npos || text.find(L"svin") != std::wstring::npos;

    // Switch out of wake-word-only recognition as soon as the trigger is heard.
    if (hasWakeWord) {
        if (isAwakeWindow) {
            awakeUntil_ = now;
            VoskEngine::Instance().SetConversationMode(false);
            VoskEngine::Instance().ResetRecognizer();
            s_lastTriggerTick = nowTick;
            DispatchVoiceAdvice(L"Хорошо, заканчиваю разговор. Скажи «Свин», когда понадоблюсь.");
            return;
        }
        awakeUntil_ = now + std::chrono::seconds(14);
        VoskEngine::Instance().SetConversationMode(true);
    }

    // Detect specific Intent / Meme
    std::wstring intent = L"";

    // TikTok 2026 Memes
    if (text.find(L"чил") != std::wstring::npos || text.find(L"чем гай") != std::wstring::npos ||
        text.find(L"чел гай") != std::wstring::npos || text.find(L"чиловый") != std::wstring::npos ||
        text.find(L"chill") != std::wstring::npos || text.find(L"на чилле") != std::wstring::npos) {
        intent = L"chill_guy";
    }
    else if (text.find(L"дубай") != std::wstring::npos || text.find(L"шоколад") != std::wstring::npos ||
             text.find(L"катаифи") != std::wstring::npos || text.find(L"фисташк") != std::wstring::npos) {
        intent = L"dubai_chocolate";
    }
    else if (text.find(L"босс") != std::wstring::npos || text.find(L"кфс") != std::wstring::npos ||
             text.find(L"kfc") != std::wstring::npos || text.find(L"52") != std::wstring::npos ||
             text.find(L"пятьдесят два") != std::wstring::npos) {
        intent = L"kfc_boss";
    }
    else if (text.find(L"хомяк") != std::wstring::npos || text.find(L"тапай") != std::wstring::npos ||
             text.find(L"тапать") != std::wstring::npos || text.find(L"хамстер") != std::wstring::npos ||
             text.find(L"листинг") != std::wstring::npos || text.find(L"эйрдроп") != std::wstring::npos) {
        intent = L"hamster";
    }
    else if (text.find(L"скибиди") != std::wstring::npos || text.find(L"туалет") != std::wstring::npos ||
             text.find(L"доп доп") != std::wstring::npos || text.find(L"skibidi") != std::wstring::npos ||
             text.find(L"брейнрот") != std::wstring::npos) {
        intent = L"skibidi";
    }
    else if (text.find(L"сигма") != std::wstring::npos || text.find(L"фонк") != std::wstring::npos ||
             text.find(L"sigma") != std::wstring::npos || text.find(L"phonk") != std::wstring::npos) {
        intent = L"sigma";
    }
    else if (text.find(L"альтушк") != std::wstring::npos || text.find(L"скуф") != std::wstring::npos ||
             text.find(L"госуслуг") != std::wstring::npos || text.find(L"скуфидон") != std::wstring::npos) {
        intent = L"altushka";
    }
    else if (text.find(L"пасхалк") != std::wstring::npos || text.find(L"медный бык") != std::wstring::npos ||
             text.find(L"1488") != std::wstring::npos || text.find(L"вентилятор") != std::wstring::npos) {
        intent = L"paskhalko";
    }
    else if (text.find(L"чиназес") != std::wstring::npos || text.find(L"саунтрес") != std::wstring::npos) {
        intent = L"chinazes";
    }
    else if (text.find(L"мьюинг") != std::wstring::npos || text.find(L"джолайн") != std::wstring::npos ||
             text.find(L"jawline") != std::wstring::npos || text.find(L"мьюинге") != std::wstring::npos) {
        intent = L"mewing";
    }
    else if (text.find(L"тяжелый люкс") != std::wstring::npos || text.find(L"олд мани") != std::wstring::npos ||
             text.find(L"люкс") != std::wstring::npos) {
        intent = L"lux";
    }
    else if (text.find(L"бархатные тяги") != std::wstring::npos || text.find(L"тяги") != std::wstring::npos ||
             text.find(L"кефтеме") != std::wstring::npos || text.find(L"фуфтыкели") != std::wstring::npos) {
        intent = L"kefteme";
    }
    else if (text.find(L"1000 минус 7") != std::wstring::npos || text.find(L"тысяча минус семь") != std::wstring::npos ||
             text.find(L"дед инсайд") != std::wstring::npos || text.find(L"гуль") != std::wstring::npos ||
             text.find(L"dead inside") != std::wstring::npos) {
        intent = L"dead_inside";
    }
    else if (text.find(L"тикток") != std::wstring::npos || text.find(L"тик ток") != std::wstring::npos ||
             text.find(L"реки") != std::wstring::npos || text.find(L"мем") != std::wstring::npos ||
             text.find(L"мэм") != std::wstring::npos) {
        intent = L"tiktok";
    }
    else if (text.find(L"гойда") != std::wstring::npos) {
        intent = L"goyda";
    }
    else if (text.find(L"виджет") != std::wstring::npos || text.find(L"дота") != std::wstring::npos) {
        intent = L"greet";
    }
    // Gameplay & Assistant commands
    else if (text.find(L"купить") != std::wstring::npos || text.find(L"закуп") != std::wstring::npos ||
             text.find(L"предмет") != std::wstring::npos || text.find(L"шмот") != std::wstring::npos ||
             text.find(L"buy") != std::wstring::npos) {
        intent = L"buy";
    }
    else if (text.find(L"качать") != std::wstring::npos || text.find(L"вкачать") != std::wstring::npos ||
             text.find(L"скилл") != std::wstring::npos || text.find(L"skill") != std::wstring::npos) {
        intent = L"skill";
    }
    else if (text.find(L"шутк") != std::wstring::npos || text.find(L"пошут") != std::wstring::npos ||
             text.find(L"анекдот") != std::wstring::npos || text.find(L"рофл") != std::wstring::npos ||
             text.find(L"прикол") != std::wstring::npos) {
        intent = L"joke";
    }
    else if (text.find(L"как дела") != std::wstring::npos || text.find(L"как сам") != std::wstring::npos ||
             text.find(L"че как") != std::wstring::npos || text.find(L"как настроение") != std::wstring::npos) {
        intent = L"howareyou";
    }
    else if (text.find(L"здаров") != std::wstring::npos || text.find(L"привет") != std::wstring::npos ||
             text.find(L"салам") != std::wstring::npos || HasWord(text, L"ку") ||
             HasWord(text, L"хай")) {
        intent = L"greet";
    }
    else if (text.find(L"красав") != std::wstring::npos || text.find(L"спасиб") != std::wstring::npos ||
             text.find(L"от души") != std::wstring::npos || text.find(L"хорош") != std::wstring::npos ||
             text.find(L"молодец") != std::wstring::npos) {
        intent = L"thanks";
    }
    else if (text.find(L"горим") != std::wstring::npos || text.find(L"проигра") != std::wstring::npos ||
             text.find(L"слива") != std::wstring::npos || text.find(L"тильт") != std::wstring::npos) {
        intent = L"tilt";
    }
    else if (text.find(L"токсик") != std::wstring::npos || text.find(L"пожестче") != std::wstring::npos ||
             text.find(L"прожар") != std::wstring::npos) {
        intent = L"toxic_on";
    }
    else if (text.find(L"кто ты") != std::wstring::npos || text.find(L"кто это") != std::wstring::npos ||
             text.find(L"ты кто") != std::wstring::npos || text.find(L"что ты") != std::wstring::npos) {
        intent = L"whoareyou";
    }
    else if (text.find(L"слышишь") != std::wstring::npos || text.find(L"слышно") != std::wstring::npos ||
             text.find(L"проверка") != std::wstring::npos || text.find(L"раз два") != std::wstring::npos ||
             text.find(L"тест") != std::wstring::npos) {
        intent = L"hearing_check";
    }
    else if (text.find(L"ты тут") != std::wstring::npos || text.find(L"ты здесь") != std::wstring::npos) {
        intent = L"whereareyou";
    }
    else if (text.find(L"по-братски") != std::wstring::npos || text.find(L"спокойн") != std::wstring::npos ||
             text.find(L"хватит") != std::wstring::npos) {
        intent = L"toxic_off";
    }

    // If the wake word was spoken by itself, acknowledge it and wait for the next phrase.
    std::wstring remainder = text;
    if (!wakeWord.empty()) {
        size_t wakePos = remainder.find(wakeWord);
        if (wakePos != std::wstring::npos) remainder.erase(wakePos, wakeWord.length());
    }
    remainder.erase(std::remove_if(remainder.begin(), remainder.end(), [](wchar_t c) {
        return iswspace(c) || c == L',' || c == L'.' || c == L'!' || c == L'?' || c == L':';
    }), remainder.end());
    if (hasWakeWord && remainder.empty()) {
        s_lastTriggerTick = nowTick;
        awakeUntil_ = now + std::chrono::seconds(14);
        VoskEngine::Instance().ResetRecognizer();
        DispatchVoiceAdvice(HeroAdvisor::Instance().GetWakePrompt());
        return;
    }

    // 1. Check AI Brain (Smart Playlist with Memory, System automation, Media control, Natural Conversation)
    bool isDirectAiCommand = (text.find(L"плейлист") != std::wstring::npos ||
                              text.find(L"плей лист") != std::wstring::npos ||
                              text.find(L"музык") != std::wstring::npos ||
                              text.find(L"песн") != std::wstring::npos ||
                              text.find(L"трек") != std::wstring::npos ||
                              text.find(L"пауза") != std::wstring::npos ||
                              text.find(L"громче") != std::wstring::npos ||
                              text.find(L"тише") != std::wstring::npos ||
                              text.find(L"доту") != std::wstring::npos ||
                              text.find(L"стим") != std::wstring::npos ||
                              text.find(L"запомни") != std::wstring::npos ||
                              text.find(L"помнишь") != std::wstring::npos ||
                              text.find(L"спотифай") != std::wstring::npos ||
                              text.find(L"яндекс") != std::wstring::npos);

    // If an explicit AI command was uttered OR wake word / active dialog window is present:
    if (isDirectAiCommand || ((hasWakeWord || isAwakeWindow) && intent.empty())) {
        std::wstring aiReply;
        if (AiBrain::Instance().ProcessSpeech(text, aiReply)) {
            s_lastTriggerTick = nowTick;
            awakeUntil_ = now + std::chrono::seconds(14);
            VoskEngine::Instance().ResetRecognizer();
            DispatchVoiceAdvice(aiReply);
            return;
        }
    }

    // 2. If an explicit intent is matched:
    if (!intent.empty()) {
        s_lastTriggerTick = nowTick;
        awakeUntil_ = now + std::chrono::seconds(12);
        VoskEngine::Instance().ResetRecognizer();

        if (intent == L"buy") { AskAdvice(0); return; }
        if (intent == L"skill") { AskAdvice(1); return; }
        if (intent == L"joke") { DispatchVoiceAdvice(HeroAdvisor::Instance().GetJoke()); return; }
        if (intent == L"howareyou") { DispatchVoiceAdvice(HeroAdvisor::Instance().GetHowAreYou()); return; }
        if (intent == L"whoareyou") { DispatchVoiceAdvice(L"Я твой верный свин-помощник! Слежу за твоей каткой в Доте и всегда на чилле!"); return; }
        if (intent == L"hearing_check") { DispatchVoiceAdvice(L"Слышу тебя громко и четко, братуха! Микрофон в идеале, я на связи!"); return; }
        if (intent == L"whereareyou") { DispatchVoiceAdvice(L"Я тут, братан! Готов разваливать трон и тапать хомяка!"); return; }
        if (intent == L"greet") { DispatchVoiceAdvice(HeroAdvisor::Instance().GetFriendlyChat()); return; }
        if (intent == L"thanks") { DispatchVoiceAdvice(HeroAdvisor::Instance().GetThanksReaction()); return; }
        if (intent == L"tilt") { DispatchVoiceAdvice(HeroAdvisor::Instance().GetCheerUp()); return; }
        if (intent == L"toxic_on") {
            ConfigManager::Get().enableToxicMode = true;
            ConfigManager::Save();
            DispatchVoiceAdvice(L"Режим жесткой прожарки включен! Держись, за каждый косяк спрошу!");
            return;
        }
        if (intent == L"toxic_off") {
            ConfigManager::Get().enableToxicMode = false;
            ConfigManager::Save();
            DispatchVoiceAdvice(L"Базара нет, братан! Выключаю токсика, общаемся по-братски и на чилле.");
            return;
        }

        // Trigger TikTok meme response
        TriggerMeme(intent);
        return;
    }

    // 3. Wake word alone (e.g. "Свин!", "Пиг!", "Брат!"):
    if (hasWakeWord) {
        s_lastTriggerTick = nowTick;
        awakeUntil_ = now + std::chrono::seconds(14);
        VoskEngine::Instance().SetConversationMode(true);
        VoskEngine::Instance().ResetRecognizer();
        DispatchVoiceAdvice(HeroAdvisor::Instance().GetWakePrompt());
        return;
    }

    // 4. Conversational follow-up: within awake window, reply to user naturally
    if (isAwakeWindow && text.length() > 2) {
        s_lastTriggerTick = nowTick;
        awakeUntil_ = now + std::chrono::seconds(14);
        VoskEngine::Instance().ResetRecognizer();
        std::wstring aiReply;
        if (AiBrain::Instance().ProcessSpeech(text, aiReply)) {
            DispatchVoiceAdvice(aiReply);
        } else {
            DispatchVoiceAdvice(HeroAdvisor::Instance().GetFriendlyChat());
        }
        return;
    }
}

void VoiceAssistant::TriggerMeme(const std::wstring& memeType) {
    std::wstring response;
    if (memeType.empty()) {
        response = HeroAdvisor::Instance().GetRandomTikTokMeme();
    } else {
        response = HeroAdvisor::Instance().GetMemeResponse(memeType);
    }
    DispatchVoiceAdvice(response);
}

void VoiceAssistant::AskAdvice(int queryType) {
    std::wstring advice;
    if (queryType == 0) {
        advice = HeroAdvisor::Instance().GetBuyAdvice();
    } else if (queryType == 1) {
        advice = HeroAdvisor::Instance().GetSkillAdvice();
    } else if (queryType == 3) {
        advice = HeroAdvisor::Instance().GetRandomTikTokMeme();
    } else {
        advice = HeroAdvisor::Instance().GetGeneralAdvice();
    }

    DispatchVoiceAdvice(advice);
}
