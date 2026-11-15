#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "voice_assistant.h"
#include "config.h"
#include "hero_advisor.h"
#include "overlay.h"
#include "voice.h"
#include <algorithm>

#pragma comment(lib, "ole32.lib")

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
    listenThread_ = std::thread(&VoiceAssistant::ListeningThread, this);
    return true;
}

void VoiceAssistant::Stop() {
    if (!running_.load()) return;
    running_.store(false);

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

    if (listenThread_.joinable()) {
        listenThread_.join();
    }
}

void VoiceAssistant::ListeningThread() {
    CoInitialize(NULL);

    // 1. Try Shared Recognizer first
    HRESULT hr = CoCreateInstance(CLSID_SpSharedRecoContext, NULL, CLSCTX_ALL, IID_ISpRecoContext, (void**)&pRecoCtx_);
    
    // 2. If Shared Recognizer failed (e.g. SPERR_SHARED_ENGINE_DISABLED 0x80045077), fallback to Inproc Recognizer with default mic!
    if (FAILED(hr) || !pRecoCtx_) {
        hr = CoCreateInstance(CLSID_SpInprocRecognizer, NULL, CLSCTX_ALL, IID_ISpRecognizer, (void**)&pInprocRec_);
        if (SUCCEEDED(hr) && pInprocRec_) {
            pInprocRec_->SetInput(NULL, TRUE); // Connect directly to default microphone endpoint
            hr = pInprocRec_->CreateRecoContext(&pRecoCtx_);
        }
    }

    if (FAILED(hr) || !pRecoCtx_) {
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

    // 3. Try loading dictation topic
    hr = pGrammar_->LoadDictation(NULL, SPLO_STATIC);
    if (SUCCEEDED(hr)) {
        pGrammar_->SetDictationState(SPRS_ACTIVE);
    }

    // 4. Also register dynamic command words via ISpGrammarBuilder
    // This allows recognition even on systems where dictation is disabled/unsupported!
    SPSTATEHANDLE hInitial = NULL;
    HRESULT hrRule = pGrammar_->GetRule(L"PigVoiceCommands", 1, SPRAF_TopLevel | SPRAF_Active, TRUE, &hInitial);
    if (SUCCEEDED(hrRule) && hInitial != NULL) {
        std::wstring customWake = ConfigManager::Get().assistantWakeWord;
        for (auto& c : customWake) c = (wchar_t)towlower(c);

        std::vector<std::wstring> phrases = {
            L"свин", L"пиг", L"свинка", L"pig", L"svin",
            L"свин что купить", L"свин что мне купить", L"свин закуп", L"свин шмот",
            L"пиг что купить", L"пиг что мне купить", L"пиг закуп", L"пиг шмот",
            L"свин что качать", L"свин что мне качать", L"свин скилл",
            L"пиг что качать", L"пиг что мне качать", L"пиг скилл",
            L"свин совет", L"пиг совет",
            L"что купить", L"что качать", L"что мне купить", L"что мне качать",
            L"pig buy", L"pig skill", L"pig help", L"svin buy", L"svin skill"
        };
        if (!customWake.empty()) {
            phrases.push_back(customWake);
            phrases.push_back(customWake + L" что купить");
            phrases.push_back(customWake + L" что качать");
            phrases.push_back(customWake + L" что мне купить");
            phrases.push_back(customWake + L" что мне качать");
            phrases.push_back(customWake + L" buy");
            phrases.push_back(customWake + L" skill");
        }

        for (const auto& phrase : phrases) {
            // Ignore errors for individual phrase transitions so one unsupported word doesn't break others
            pGrammar_->AddWordTransition(hInitial, NULL, phrase.c_str(), L" ", SPWT_LEXICAL, 1.0f, NULL);
        }
        pGrammar_->Commit(0);
        pGrammar_->SetRuleState(L"PigVoiceCommands", NULL, SPRS_ACTIVE);
    }

    pRecoCtx_->SetInterest(SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS), SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_HYPOTHESIS));
    pRecoCtx_->Resume(0);

    HANDLE hEvent = pRecoCtx_->GetNotifyEventHandle();

    while (running_.load()) {
        if (hEvent != NULL) {
            DWORD waitRes = WaitForSingleObject(hEvent, 250);
            if (waitRes == WAIT_OBJECT_0) {
                SPEVENT spEvent;
                ULONG fetched = 0;
                while (pRecoCtx_->GetEvents(1, &spEvent, &fetched) == S_OK && fetched > 0) {
                    if (spEvent.eEventId == SPEI_RECOGNITION) {
                        ISpRecoResult* pResult = reinterpret_cast<ISpRecoResult*>(spEvent.lParam);
                        if (pResult) {
                            WCHAR* pwszText = NULL;
                            if (SUCCEEDED(pResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &pwszText, NULL))) {
                                if (pwszText) {
                                    ProcessRecognizedText(pwszText);
                                    CoTaskMemFree(pwszText);
                                }
                            }
                        }
                    }
                }
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
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

void VoiceAssistant::ProcessRecognizedText(const std::wstring& rawText) {
    std::wstring text = rawText;
    for (auto& ch : text) ch = (wchar_t)towlower(ch);

    std::wstring wakeWord = ConfigManager::Get().assistantWakeWord;
    for (auto& ch : wakeWord) ch = (wchar_t)towlower(ch);

    // Check if phrase contains wake word or direct question
    bool hasWakeWord = false;
    if (!wakeWord.empty() && text.find(wakeWord) != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"свин") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"пиг") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"свинка") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"pig") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"svin") != std::wstring::npos) hasWakeWord = true;

    bool directQuestion = (text.find(L"что купить") != std::wstring::npos ||
                           text.find(L"что мне купить") != std::wstring::npos ||
                           text.find(L"что качать") != std::wstring::npos ||
                           text.find(L"что мне качать") != std::wstring::npos);

    bool isFriendGreeting = (text.find(L"здаров") != std::wstring::npos ||
                             text.find(L"привет") != std::wstring::npos ||
                             text.find(L"салам") != std::wstring::npos ||
                             text.find(L"братан") != std::wstring::npos ||
                             text.find(L"братух") != std::wstring::npos ||
                             text.find(L"брат") != std::wstring::npos ||
                             text.find(L"газяв") != std::wstring::npos ||
                             text.find(L"как дела") != std::wstring::npos ||
                             text.find(L"hello") != std::wstring::npos ||
                             text.find(L"hi") != std::wstring::npos);

    if (isFriendGreeting) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetFriendlyChat());
        return;
    }

    if (!hasWakeWord && !directQuestion) return;

    // Check intent
    if (text.find(L"купить") != std::wstring::npos ||
        text.find(L"закуп") != std::wstring::npos ||
        text.find(L"предмет") != std::wstring::npos ||
        text.find(L"шмот") != std::wstring::npos ||
        text.find(L"buy") != std::wstring::npos) {
        AskAdvice(0); // Buy
    } else if (text.find(L"качать") != std::wstring::npos ||
               text.find(L"вкачать") != std::wstring::npos ||
               text.find(L"скилл") != std::wstring::npos ||
               text.find(L"способност") != std::wstring::npos ||
               text.find(L"skill") != std::wstring::npos) {
        AskAdvice(1); // Skill
    } else {
        // Calling his name (e.g. "Свин!" or "Пиг!") -> friendly bro chat!
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetFriendlyChat());
    }
}

void VoiceAssistant::AskAdvice(int queryType) {
    std::wstring advice;
    if (queryType == 0) {
        advice = HeroAdvisor::Instance().GetBuyAdvice();
    } else if (queryType == 1) {
        advice = HeroAdvisor::Instance().GetSkillAdvice();
    } else {
        advice = HeroAdvisor::Instance().GetGeneralAdvice();
    }

    // Show pig speech bubble & voice TTS
    OverlayWindow::Instance().Show(true);
    OverlayWindow::Instance().TriggerAdvice(advice);
}
