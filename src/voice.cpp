#include "voice.h"
#include "config.h"

#pragma comment(lib, "ole32.lib")

VoiceService& VoiceService::Instance() {
    static VoiceService instance;
    return instance;
}

VoiceService::VoiceService() {}

VoiceService::~VoiceService() {
    Stop();
}

void VoiceService::Init() {
    if (initialized_) return;
    CoInitialize(NULL);

    HRESULT hr = CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice_);
    if (SUCCEEDED(hr) && pVoice_) {
        initialized_ = true;
        ApplyVoiceSettings();
    }
}

void VoiceService::ApplyVoiceSettings() {
    if (!pVoice_) return;
    int targetGender = ConfigManager::Get().voiceGender; // 0 = Female (Irina), 1 = Male (Pavel)

    bool voiceSet = false;
    ISpObjectTokenCategory* pCat = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL, IID_ISpObjectTokenCategory, (void**)&pCat);
    if (SUCCEEDED(hr) && pCat) {
        pCat->SetId(L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices", FALSE);
        IEnumSpObjectTokens* pEnum = nullptr;
        if (SUCCEEDED(pCat->EnumTokens(NULL, NULL, &pEnum)) && pEnum) {
            ISpObjectToken* pToken = nullptr;
            while (pEnum->Next(1, &pToken, NULL) == S_OK) {
                WCHAR* desc = nullptr;
                pToken->GetStringValue(NULL, &desc);
                if (desc) {
                    std::wstring wdesc(desc);
                    bool match = false;
                    if (targetGender == 1) { // Male (Pavel)
                        if (wdesc.find(L"Pavel") != std::wstring::npos || 
                           (wdesc.find(L"Male") != std::wstring::npos && wdesc.find(L"Russian") != std::wstring::npos)) {
                            match = true;
                        }
                    } else { // Female (Irina)
                        if (wdesc.find(L"Irina") != std::wstring::npos || 
                           (wdesc.find(L"Female") != std::wstring::npos && wdesc.find(L"Russian") != std::wstring::npos)) {
                            match = true;
                        }
                    }

                    if (match) {
                        if (SUCCEEDED(pVoice_->SetVoice(pToken))) {
                            voiceSet = true;
                        }
                    }
                    CoTaskMemFree(desc);
                }
                pToken->Release();
                if (voiceSet) break;
            }
            pEnum->Release();
        }
        pCat->Release();
    }

    // Fallback: If target voice not found in OneCore, search classic SAPI voices
    if (!voiceSet) {
        ISpObjectTokenCategory* pSapiCat = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_SpObjectTokenCategory, NULL, CLSCTX_ALL, IID_ISpObjectTokenCategory, (void**)&pSapiCat)) && pSapiCat) {
            pSapiCat->SetId(SPCAT_VOICES, FALSE);
            IEnumSpObjectTokens* pEnum = nullptr;
            if (SUCCEEDED(pSapiCat->EnumTokens(NULL, NULL, &pEnum)) && pEnum) {
                ISpObjectToken* pToken = nullptr;
                while (pEnum->Next(1, &pToken, NULL) == S_OK) {
                    WCHAR* desc = nullptr;
                    pToken->GetStringValue(NULL, &desc);
                    if (desc) {
                        std::wstring wdesc(desc);
                        if (wdesc.find(L"Russian") != std::wstring::npos || wdesc.find(L"Irina") != std::wstring::npos) {
                            pVoice_->SetVoice(pToken);
                            voiceSet = true;
                        }
                        CoTaskMemFree(desc);
                    }
                    pToken->Release();
                    if (voiceSet) break;
                }
                pEnum->Release();
            }
            pSapiCat->Release();
        }
    }

    // Set natural, expressive speaking rate (+1) and user volume
    pVoice_->SetRate(1);
    pVoice_->SetVolume((USHORT)ConfigManager::Get().soundVolume);
    currentGender_ = targetGender;
}

void VoiceService::Speak(const std::wstring& text) {
    if (!ConfigManager::Get().enableVoiceTTS) return;
    if (!initialized_) Init();
    if (pVoice_) {
        // If voice gender changed in settings, reapply
        if (currentGender_ != ConfigManager::Get().voiceGender) {
            ApplyVoiceSettings();
        }
        pVoice_->Speak(text.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, NULL);
    }
}

bool VoiceService::IsSpeaking() const {
    if (!pVoice_) return false;
    SPVOICESTATUS status;
    if (SUCCEEDED(pVoice_->GetStatus(&status, NULL))) {
        return (status.dwRunningState == SPRS_IS_SPEAKING);
    }
    return false;
}

void VoiceService::Stop() {
    if (pVoice_) {
        pVoice_->Release();
        pVoice_ = nullptr;
    }
    if (initialized_) {
        CoUninitialize();
        initialized_ = false;
    }
}
