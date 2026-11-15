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
#include <vector>
#include <chrono>

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

bool VoiceAssistant::IsAwake() const {
    return (std::chrono::steady_clock::now() < awakeUntil_);
}

void VoiceAssistant::ListeningThread() {
    CoInitialize(NULL);

    // 1. Try Shared Recognizer first
    HRESULT hr = CoCreateInstance(CLSID_SpSharedRecoContext, NULL, CLSCTX_ALL, IID_ISpRecoContext, (void**)&pRecoCtx_);
    
    // 2. If Shared Recognizer failed, fallback to Inproc Recognizer with default microphone
    if (FAILED(hr) || !pRecoCtx_) {
        hr = CoCreateInstance(CLSID_SpInprocRecognizer, NULL, CLSCTX_ALL, IID_ISpRecognizer, (void**)&pInprocRec_);
        if (SUCCEEDED(hr) && pInprocRec_) {
            pInprocRec_->SetInput(NULL, TRUE); // Default mic
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

    // 4. Register command grammar with Russian and phonetic/English words
    SPSTATEHANDLE hInitial = NULL;
    HRESULT hrRule = pGrammar_->GetRule(L"PigVoiceCommands", 1, SPRAF_TopLevel | SPRAF_Active, TRUE, &hInitial);
    if (SUCCEEDED(hrRule) && hInitial != NULL) {
        std::wstring customWake = ConfigManager::Get().assistantWakeWord;
        for (auto& c : customWake) c = (wchar_t)towlower(c);

        std::vector<std::wstring> phrases = {
            // Wake words (Russian, phonetic, translit)
            L"свин", L"пиг", L"свинка", L"хряк", L"боров", L"братуха", L"братан",
            L"pig", L"svin", L"sveen", L"spin", L"swine", L"brother", L"bro",

            // Greetings
            L"свин здарова", L"свин привет", L"свин салам", L"пиг здарова", L"пиг привет",
            L"здарова братан", L"здарова братух", L"здарова свин", L"привет свин",
            L"здарова", L"привет", L"салам", L"ку", L"хай",
            L"svin zdarova", L"pig hello", L"pig hi", L"hello", L"hi",
            L"zdarova", L"privet", L"salam", L"the rover", L"starova",

            // How are you
            L"свин как дела", L"свин как сам", L"свин че как", L"как дела свин", L"как сам свин",
            L"как дела", L"как сам", L"че как", L"как настроение",
            L"svin kak dela", L"pig how are you", L"how are you", L"whats up", L"cock dela",

            // Jokes & humor
            L"свин пошути", L"свин расскажи шутку", L"свин анекдот", L"пошути свин",
            L"пиг пошути", L"пиг анекдот", L"расскажи шутку", L"пошути", L"анекдот",
            L"svin shutka", L"svin anekdot", L"pig joke", L"tell a joke", L"joke", L"shootka",

            // What to buy
            L"свин что купить", L"свин что мне купить", L"свин закуп", L"свин шмот", L"свин какой закуп",
            L"пиг что купить", L"пиг что мне купить", L"пиг закуп", L"пиг шмот",
            L"что купить", L"что мне купить", L"какой закуп", L"закуп", L"шмот",
            L"pig buy", L"svin buy", L"what to buy", L"chto kupit", L"sto kupit",

            // What to skill
            L"свин что качать", L"свин что мне качать", L"свин скилл", L"свин какой скилл",
            L"пиг что качать", L"пиг что мне качать", L"пиг скилл",
            L"что качать", L"что мне качать", L"какой скилл", L"скиллы",
            L"pig skill", L"svin skill", L"what to skill", L"chto kachat",

            // Thanks & praise
            L"свин красава", L"свин спасибо", L"свин от души", L"свин молодец", L"свин хорош",
            L"пиг красава", L"пиг спасибо", L"красава свин", L"спасибо свин",
            L"красава", L"спасибо", L"от души", L"хорош", L"молодец",
            L"krasava", L"spasibo", L"thanks", L"thank you", L"good job",

            // Anti-tilt & help
            L"свин что делать", L"свин мы горим", L"свин мы проиграем", L"мы горим", L"мы проиграем", L"тильт",
            L"chto delat", L"help",

            // Personality style switch
            L"свин токсик", L"свин пожестче", L"свин прожарь", L"свин по-братски", L"свин спокойнее"
        };

        if (!customWake.empty()) {
            phrases.push_back(customWake);
            phrases.push_back(customWake + L" что купить");
            phrases.push_back(customWake + L" что качать");
            phrases.push_back(customWake + L" как дела");
            phrases.push_back(customWake + L" пошути");
            phrases.push_back(customWake + L" анекдот");
            phrases.push_back(customWake + L" здарова");
            phrases.push_back(customWake + L" красава");
            phrases.push_back(customWake + L" buy");
            phrases.push_back(customWake + L" skill");
            phrases.push_back(customWake + L" joke");
        }

        for (const auto& phrase : phrases) {
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

    auto now = std::chrono::steady_clock::now();
    bool isAwakeWindow = (now < awakeUntil_);

    std::wstring wakeWord = ConfigManager::Get().assistantWakeWord;
    for (auto& ch : wakeWord) ch = (wchar_t)towlower(ch);

    // 1. Check Wake Word presence
    bool hasWakeWord = false;
    if (!wakeWord.empty() && text.find(wakeWord) != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"свин") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"пиг") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"свинка") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"хряк") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"боров") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"братух") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"братан") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"pig") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"svin") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"spin") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"sveen") != std::wstring::npos) hasWakeWord = true;
    if (text.find(L"swine") != std::wstring::npos) hasWakeWord = true;

    // If pig was awakened recently (< 10 sec ago), he is actively listening without needing wake word!
    bool canProceed = hasWakeWord || isAwakeWindow;

    // Direct questions/intents always work even without wake word
    if (text.find(L"что купить") != std::wstring::npos ||
        text.find(L"что качать") != std::wstring::npos ||
        text.find(L"пошути") != std::wstring::npos ||
        text.find(L"анекдот") != std::wstring::npos ||
        text.find(L"как дела") != std::wstring::npos ||
        text.find(L"здарова") != std::wstring::npos ||
        text.find(L"красава") != std::wstring::npos) {
        canProceed = true;
    }

    if (!canProceed) return;

    // Refresh active listening window for 10 seconds!
    awakeUntil_ = now + std::chrono::seconds(10);

    // Intent 1: Switch toxic mode on / off dynamically by voice!
    if (text.find(L"токсик") != std::wstring::npos ||
        text.find(L"пожестче") != std::wstring::npos ||
        text.find(L"прожар") != std::wstring::npos ||
        text.find(L"toxic") != std::wstring::npos) {
        ConfigManager::Get().enableToxicMode = true;
        ConfigManager::Save();
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(L"Режим жесткой прожарки включен! Теперь держись, пощады не будет, за каждый косяк спрошу!");
        return;
    }
    if (text.find(L"по-братски") != std::wstring::npos ||
        text.find(L"спокойн") != std::wstring::npos ||
        text.find(L"хватит") != std::wstring::npos ||
        text.find(L"добр") != std::wstring::npos) {
        ConfigManager::Get().enableToxicMode = false;
        ConfigManager::Save();
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(L"Базара нет, братан! Выключаю токсика, общаемся по-братски и на чилле.");
        return;
    }

    // Intent 2: Jokes & Humor ("пошути", "анекдот", "шутка", "рофл")
    if (text.find(L"шутк") != std::wstring::npos ||
        text.find(L"пошут") != std::wstring::npos ||
        text.find(L"анекдот") != std::wstring::npos ||
        text.find(L"рофл") != std::wstring::npos ||
        text.find(L"прикол") != std::wstring::npos ||
        text.find(L"рассмеши") != std::wstring::npos ||
        text.find(L"joke") != std::wstring::npos ||
        text.find(L"funny") != std::wstring::npos ||
        text.find(L"shoot") != std::wstring::npos) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetJoke());
        return;
    }

    // Intent 3: How are you ("как дела", "как сам", "че как", "че делаешь")
    if (text.find(L"как дела") != std::wstring::npos ||
        text.find(L"как сам") != std::wstring::npos ||
        text.find(L"че как") != std::wstring::npos ||
        text.find(L"че делаешь") != std::wstring::npos ||
        text.find(L"как настроение") != std::wstring::npos ||
        text.find(L"how are you") != std::wstring::npos ||
        text.find(L"whats up") != std::wstring::npos ||
        text.find(L"kak dela") != std::wstring::npos ||
        text.find(L"cock dela") != std::wstring::npos) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetHowAreYou());
        return;
    }

    // Intent 4: Greetings ("здарова", "привет", "салам", "газяву взял")
    if (text.find(L"здаров") != std::wstring::npos ||
        text.find(L"привет") != std::wstring::npos ||
        text.find(L"салам") != std::wstring::npos ||
        text.find(L"ку") != std::wstring::npos ||
        text.find(L"хай") != std::wstring::npos ||
        text.find(L"газяв") != std::wstring::npos ||
        text.find(L"добрый день") != std::wstring::npos ||
        text.find(L"hello") != std::wstring::npos ||
        text.find(L"hi") != std::wstring::npos ||
        text.find(L"rover") != std::wstring::npos ||
        text.find(L"starova") != std::wstring::npos) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetFriendlyChat());
        return;
    }

    // Intent 5: Thanks & praise ("красава", "спасибо", "от души", "хорош")
    if (text.find(L"красав") != std::wstring::npos ||
        text.find(L"спасиб") != std::wstring::npos ||
        text.find(L"спс") != std::wstring::npos ||
        text.find(L"от души") != std::wstring::npos ||
        text.find(L"хорош") != std::wstring::npos ||
        text.find(L"молодец") != std::wstring::npos ||
        text.find(L"thanks") != std::wstring::npos ||
        text.find(L"thank you") != std::wstring::npos ||
        text.find(L"good job") != std::wstring::npos) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetThanksReaction());
        return;
    }

    // Intent 6: Anti-tilt / Motivation ("мы горим", "мы проиграем", "тильт", "что делать")
    if (text.find(L"горим") != std::wstring::npos ||
        text.find(L"проигра") != std::wstring::npos ||
        text.find(L"слива") != std::wstring::npos ||
        text.find(L"тильт") != std::wstring::npos ||
        text.find(L"раки") != std::wstring::npos ||
        text.find(L"нубы") != std::wstring::npos) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetCheerUp());
        return;
    }

    // Intent 7: Buy advice ("что купить", "закуп", "предмет", "шмот", "buy")
    if (text.find(L"купить") != std::wstring::npos ||
        text.find(L"закуп") != std::wstring::npos ||
        text.find(L"предмет") != std::wstring::npos ||
        text.find(L"шмот") != std::wstring::npos ||
        text.find(L"айтем") != std::wstring::npos ||
        text.find(L"buy") != std::wstring::npos) {
        AskAdvice(0); // Buy
        return;
    }

    // Intent 8: Skill advice ("что качать", "скилл", "прокачка", "способност")
    if (text.find(L"качать") != std::wstring::npos ||
        text.find(L"вкачать") != std::wstring::npos ||
        text.find(L"скилл") != std::wstring::npos ||
        text.find(L"способност") != std::wstring::npos ||
        text.find(L"прокачк") != std::wstring::npos ||
        text.find(L"skill") != std::wstring::npos) {
        AskAdvice(1); // Skill
        return;
    }

    // Intent 9: Wake word alone ("Свин!" / "Пиг!")
    if (hasWakeWord) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetWakePrompt());
        return;
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
