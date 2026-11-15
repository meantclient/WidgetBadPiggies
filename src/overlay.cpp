#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "overlay.h"
#include "config.h"
#include "dota_gsi.h"
#include "dota_settings_gui.h"
#include "voice.h"
#include "voice_assistant.h"
#include "hero_advisor.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <mmsystem.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib")

using namespace Gdiplus;

static const wchar_t* OVERLAY_CLASS = L"BadPiggiesOverlayClass";

OverlayWindow& OverlayWindow::Instance() {
    static OverlayWindow instance;
    return instance;
}

OverlayWindow::OverlayWindow() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, NULL);
    srand((unsigned int)time(NULL));

    killPhrases_ = {
        L"Красава! Минус один!",
        L"Молодец! Отличный килл!",
        L"Вот это скилл!",
        L"Хрю! Делаешь вещи!",
        L"Изи для тебя!",
        L"Красава! Дави их!"
    };

    streakPhrases_ = {
        L"РАМПАГА!",
        L"ДА ТЫ МОНСТР!",
        L"ДОМИНИРОВАНИЕ!",
        L"КТО ОСТАНОВИТ ЭТУ СВИНКУ?!"
    };

    deathPhrases_ = {
        L"Ой-ой... Тебя убили...",
        L"Виноват саппорт, точно!",
        L"В следующий раз заберем!",
        L"Хрюк... Какая досада!"
    };

    lowHpPhrases_ = {
        L"МАЛО ХП! ЖМИ БКБ!",
        L"ОПАСНО! ХРЮ-Ю! ЛЕЧИСЬ!",
        L"БЕГИ, ФОРЕСТ, БЕГИ!",
        L"СЕЙВИТЕСЬ, ГЛУПЦЫ!"
    };

    lotusPhrases_ = {
        L"Лотус созрел! Забери его!",
        L"Лотусы появились! Беги к пруду!",
        L"Лотус готов! Не отдавай врагам!",
        L"Хватай лотус скорее!"
    };

    LoadCustomFont();
}

OverlayWindow::~OverlayWindow() {
    Destroy();
    if (gdiplusToken_) {
        GdiplusShutdown(gdiplusToken_);
        gdiplusToken_ = 0;
    }
}

bool OverlayWindow::Create() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = OverlayWindow::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = OVERLAY_CLASS;
    RegisterClassExW(&wc);

    // 1. Initial dimensions & position
    winWidth_ = std::max(460, ConfigManager::Get().pigSize + 220);
    winHeight_ = std::max(380, ConfigManager::Get().pigSize + 180);

    isDynamicIsland_ = ConfigManager::Get().enableDynamicIsland;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    if (isDynamicIsland_) {
        posX_ = (screenW - winWidth_) / 2;
        posY_ = 0;
    } else if (ConfigManager::Get().posX >= 0 && ConfigManager::Get().posY >= 0) {
        posX_ = std::max(0, std::min(ConfigManager::Get().posX, screenW - winWidth_));
        posY_ = std::max(0, std::min(ConfigManager::Get().posY, screenH - winHeight_));
    } else {
        posX_ = screenW - winWidth_ - 30;
        posY_ = screenH - winHeight_ - 50;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        OVERLAY_CLASS,
        L"Bad Piggies Overlay",
        WS_POPUP,
        posX_, posY_, winWidth_, winHeight_,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    if (!hwnd_) return false;

    LoadCurrentSkin();
    SetTimer(hwnd_, 1, 16, NULL); // ~60 FPS

    bool dotaOn = DotaGsiService::Instance().IsDotaProcessRunning();
    if (ConfigManager::Get().hideWhenNoDota) {
        Show(dotaOn);
    } else {
        Show(true);
    }
    return true;
}

void OverlayWindow::Destroy() {
    if (hwnd_) {
        KillTimer(hwnd_, 1);
        DestroyWindow(hwnd_);
        hwnd_ = NULL;
    }
}

void OverlayWindow::Show(bool show) {
    visible_ = show;
    if (hwnd_) {
        if (show) {
            SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        } else {
            ShowWindow(hwnd_, SW_HIDE);
            SetWindowPos(hwnd_, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_HIDEWINDOW);
        }
    }
}

void OverlayWindow::LoadCurrentSkin() {
    int pigType = ConfigManager::Get().pigType;
    std::wstring imgFile = L"pig_" + std::to_wstring(pigType) + L".png";
    std::wstring fullPath = ConfigManager::GetAssetPath(imgFile);

    currentPigImage_ = std::make_unique<Image>(fullPath.c_str());
    if (currentPigImage_->GetLastStatus() != Ok) {
        currentPigImage_.reset();
    }
    loadedPigType_ = pigType;

    // Load beaten pig
    std::wstring beatenPath = ConfigManager::GetAssetPath(L"pig_beaten.png");
    beatenPigImage_ = std::make_unique<Image>(beatenPath.c_str());
    if (beatenPigImage_->GetLastStatus() != Ok) {
        beatenPigImage_.reset();
    }
}

void OverlayWindow::LoadCustomFont() {
    std::wstring fontPath = ConfigManager::GetAssetPath(L"aminahs_schoolbook_bd.ttf");
    AddFontResourceExW(fontPath.c_str(), FR_PRIVATE, 0);

    fontCollection_ = std::make_unique<PrivateFontCollection>();
    if (fontCollection_->AddFontFile(fontPath.c_str()) == Ok) {
        int count = fontCollection_->GetFamilyCount();
        if (count > 0) {
            std::vector<FontFamily> families(count);
            int numFound = 0;
            fontCollection_->GetFamilies(count, families.data(), &numFound);
            wchar_t familyName[LF_FACESIZE] = { 0 };
            families[0].GetFamilyName(familyName);
            customFontName_ = familyName;
        }
    }
}

void OverlayWindow::ReloadSkinAndSize() {
    LoadCurrentSkin();

    int pigSize = ConfigManager::Get().pigSize;
    winWidth_ = std::max(460, pigSize + 220);
    winHeight_ = std::max(380, pigSize + 180);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    if (isDynamicIsland_) {
        posX_ = (screenW - winWidth_) / 2;
        posY_ = 0;
    } else if (ConfigManager::Get().posX >= 0 && ConfigManager::Get().posY >= 0) {
        posX_ = std::max(0, std::min(ConfigManager::Get().posX, screenW - winWidth_));
        posY_ = std::max(0, std::min(ConfigManager::Get().posY, screenH - winHeight_));
    } else {
        posX_ = screenW - winWidth_ - 30;
        posY_ = screenH - winHeight_ - 50;
    }

    if (hwnd_) {
        SetWindowPos(hwnd_, HWND_TOPMOST, posX_, posY_, winWidth_, winHeight_, SWP_NOACTIVATE | SWP_NOZORDER);
    }
}

void OverlayWindow::SetDynamicIslandMode(bool enable) {
    if (isDynamicIsland_ == enable) return;
    isDynamicIsland_ = enable;
    ConfigManager::Get().enableDynamicIsland = enable;
    ConfigManager::Save();

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    if (enable) {
        savedPigX_ = posX_;
        savedPigY_ = posY_;
        posX_ = (screenW - winWidth_) / 2;
        posY_ = 0;
        islandCurrentWidth_ = 220.0f;
        islandTargetWidth_ = 220.0f;
        islandCurrentHeight_ = 38.0f;
        islandTargetHeight_ = 38.0f;
        PlayAudio(L"lotus.wav");
    } else {
        if (savedPigX_ >= 0 && savedPigY_ >= 0) {
            posX_ = savedPigX_;
            posY_ = savedPigY_;
        } else {
            posX_ = screenW - winWidth_ - 30;
            posY_ = GetSystemMetrics(SM_CYSCREEN) - winHeight_ - 50;
        }
        PlayAudio(L"kill.wav");
    }
    if (hwnd_) {
        SetWindowPos(hwnd_, HWND_TOPMOST, posX_, posY_, winWidth_, winHeight_, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
    Render();
}

void OverlayWindow::PlayAudio(const std::wstring& filename) {
    if (!ConfigManager::Get().enableSound) return;
    if (filename == L"kill.wav" && !ConfigManager::Get().soundKill) return;
    if (filename == L"death.wav" && !ConfigManager::Get().soundDeath) return;
    if (filename == L"low_hp.wav" && !ConfigManager::Get().soundLowHp) return;
    if (filename == L"lotus.wav" && !ConfigManager::Get().soundLotus) return;

    std::wstring path = ConfigManager::GetAssetPath(L"sounds\\" + filename);
    if (PathFileExistsW(path.c_str())) {
        PlaySoundW(path.c_str(), NULL, SND_ASYNC | SND_FILENAME | SND_NODEFAULT);
    }
}

void OverlayWindow::TriggerKill(int kills, int streak) {
    state_ = PigAnimationState::JUMP_KILL;
    animTimer_ = 0.0;
    stateDuration_ = 3.8;

    // Track rapid multi-kills (within 18 seconds window like Dota 2)
    static DWORD lastKillTick = 0;
    static int multiKills = 0;
    DWORD now = GetTickCount();
    if (lastKillTick != 0 && (now - lastKillTick <= 18000)) {
        multiKills++;
    } else {
        multiKills = 1;
    }
    lastKillTick = now;

    if (ConfigManager::Get().enableToxicMode) {
        bubbleText_ = HeroAdvisor::Instance().GetKillComment(std::max(streak, multiKills));
    } else if (multiKills >= 5 || streak >= 5) {
        bubbleText_ = L"РАМПАГА!";
    } else if (multiKills == 4) {
        bubbleText_ = L"УЛЬТРА КИЛЛ!";
    } else if (multiKills == 3) {
        bubbleText_ = L"ТРИПЛ КИЛЛ!";
    } else if (multiKills == 2) {
        bubbleText_ = L"ДАБЛ КИЛЛ!";
    } else if (streak >= 3) {
        int idx = rand() % streakPhrases_.size();
        bubbleText_ = streakPhrases_[idx];
    } else {
        // Pick either the special "Красава, {Name}!" phrase OR one of the other kill phrases
        int r = rand() % (killPhrases_.size() + 1);
        if (r == 0) {
            if (ConfigManager::Get().usePlayerName && !ConfigManager::Get().playerName.empty()) {
                bubbleText_ = L"Красава, " + ConfigManager::Get().playerName + L"!";
            } else {
                bubbleText_ = L"Красава!";
            }
        } else {
            bubbleText_ = killPhrases_[r - 1];
        }
    }

    PlayAudio(L"kill.wav");
    VoiceService::Instance().Speak(bubbleText_);
}

void OverlayWindow::TriggerDeath() {
    state_ = PigAnimationState::SAD_DEATH;
    animTimer_ = 0.0;
    stateDuration_ = 3.8;

    if (ConfigManager::Get().enableToxicMode) {
        bubbleText_ = HeroAdvisor::Instance().GetDeathRoast();
    } else {
        int idx = rand() % deathPhrases_.size();
        bubbleText_ = deathPhrases_[idx];
    }

    PlayAudio(L"death.wav");
    VoiceService::Instance().Speak(bubbleText_);
}

void OverlayWindow::TriggerLotusPre(bool isTurbo) {
    if (!ConfigManager::Get().enableLotusReminder) return;

    state_ = PigAnimationState::LOTUS_REMINDER;
    animTimer_ = 0.0;
    stateDuration_ = 4.2;

    if (isTurbo) {
        bubbleText_ = L"Турбо-лотус через 10 сек!";
        PlayAudio(L"lotus.wav");
        VoiceService::Instance().Speak(L"Турбо лотус через десять секунд! Беги к пруду!");
    } else {
        bubbleText_ = L"Лотус через 15 сек! Беги к пруду!";
        PlayAudio(L"lotus.wav");
        VoiceService::Instance().Speak(L"Лотус через пятнадцать секунд! Беги к пруду!");
    }
}

void OverlayWindow::TriggerLotus(bool isTurbo) {
    if (!ConfigManager::Get().enableLotusReminder) return;

    state_ = PigAnimationState::LOTUS_REMINDER;
    animTimer_ = 0.0;
    stateDuration_ = 4.2;

    if (ConfigManager::Get().enableToxicMode) {
        bubbleText_ = HeroAdvisor::Instance().GetLotusComment(isTurbo);
        PlayAudio(L"lotus.wav");
        VoiceService::Instance().Speak(bubbleText_);
    } else if (isTurbo) {
        bubbleText_ = L"Турбо-лотус созрел! Забирай!";
        PlayAudio(L"lotus.wav");
        VoiceService::Instance().Speak(L"Турбо лотус созрел! Забирай!");
    } else {
        int idx = rand() % lotusPhrases_.size();
        bubbleText_ = lotusPhrases_[idx];
        PlayAudio(L"lotus.wav");
        VoiceService::Instance().Speak(bubbleText_);
    }
}

void OverlayWindow::TriggerAdvice(const std::wstring& advice) {
    state_ = PigAnimationState::JUMP_KILL;
    animTimer_ = 0.0;
    stateDuration_ = 6.5;
    bubbleText_ = advice;

    PlayAudio(L"lotus.wav");
    VoiceService::Instance().Speak(advice);
}

void OverlayWindow::TriggerLowHp(bool isLow, int hpPercent) {
    if (isLow) {
        if (state_ != PigAnimationState::PANIC_LOW_HP) {
            state_ = PigAnimationState::PANIC_LOW_HP;
            animTimer_ = 0.0;
            stateDuration_ = 99999.0;
            int idx = rand() % lowHpPhrases_.size();
            bubbleText_ = lowHpPhrases_[idx];
            PlayAudio(L"low_hp.wav");
            VoiceService::Instance().Speak(bubbleText_);
            lastLowHpSoundTime_ = 0.0;
        }
    } else {
        if (state_ == PigAnimationState::PANIC_LOW_HP) {
            state_ = PigAnimationState::IDLE;
            animTimer_ = 0.0;
            bubbleText_ = L"";
        }
    }
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* self = (OverlayWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return 0;
    }
    case WM_TIMER:
        if (self) self->OnTimer();
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OverlayWindow::OnTimer() {
    const double dt = 0.016;
    animTimer_ += dt;

    // Dynamic Island smooth animation
    if (isDynamicIsland_) {
        if (!bubbleText_.empty()) {
            islandTargetWidth_ = std::min(460.0f, std::max(280.0f, (float)bubbleText_.length() * 9.0f + 110.0f));
            islandTargetHeight_ = 48.0f;
        } else {
            islandTargetWidth_ = 220.0f;
            islandTargetHeight_ = 38.0f;
        }
        islandCurrentWidth_ += (islandTargetWidth_ - islandCurrentWidth_) * 0.18f;
        islandCurrentHeight_ += (islandTargetHeight_ - islandCurrentHeight_) * 0.18f;
    }

    // 1. Mouse interactions (Smooth Dragging with Multi-Monitor, Snap to Dynamic Island)
    POINT pt;
    GetCursorPos(&pt);
    RECT rc;
    GetWindowRect(hwnd_, &rc);
    bool mouseOver = (hwnd_ && visible_ && PtInRect(&rc, pt));

    bool lbutton = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    bool ctrlDown = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;

    // Start drag on click over pig or with Ctrl
    if (!isDragging_ && mouseOver && (lbutton || (ctrlDown && lbutton))) {
        isDragging_ = true;
        dragStartMouse_ = pt;
        dragStartPos_ = POINT{ posX_, posY_ };
        LONG ex = GetWindowLong(hwnd_, GWL_EXSTYLE);
        if (ex & WS_EX_TRANSPARENT) {
            SetWindowLong(hwnd_, GWL_EXSTYLE, ex & ~WS_EX_TRANSPARENT);
        }
    }

    if (isDragging_) {
        if (lbutton) {
            int deltaX = pt.x - dragStartMouse_.x;
            int deltaY = pt.y - dragStartMouse_.y;

            // Multi-monitor virtual bounds (free drag, no restrictions or sudden drops!)
            int minX = GetSystemMetrics(SM_XVIRTUALSCREEN) - winWidth_ / 2;
            int maxX = GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) - winWidth_ / 2;
            int minY = -15; // allow reaching very top edge!
            int maxY = GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN) - 70;

            int startX = (int)dragStartPos_.x;
            int startY = (int)dragStartPos_.y;
            int newX = std::max(minX, std::min(startX + deltaX, maxX));
            int newY = std::max(minY, std::min(startY + deltaY, maxY));

            // SNAP TO DYNAMIC ISLAND when lifted to the very top edge!
            if (!isDynamicIsland_ && pt.y <= 24) {
                SetDynamicIslandMode(true);
                isDragging_ = false;
                LONG ex = GetWindowLong(hwnd_, GWL_EXSTYLE);
                if (!(ex & WS_EX_TRANSPARENT)) {
                    SetWindowLong(hwnd_, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
                }
                return;
            }

            // DETACH FROM DYNAMIC ISLAND when dragged down!
            if (isDynamicIsland_ && deltaY > 30) {
                SetDynamicIslandMode(false);
                posX_ = pt.x - winWidth_ / 2;
                posY_ = pt.y - winHeight_ / 2;
                dragStartMouse_ = pt;
                dragStartPos_ = POINT{ posX_, posY_ };
                SetWindowPos(hwnd_, HWND_TOPMOST, posX_, posY_, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            } else if (!isDynamicIsland_) {
                posX_ = newX;
                posY_ = newY;
                SetWindowPos(hwnd_, HWND_TOPMOST, posX_, posY_, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            }
        } else {
            // Drag released
            isDragging_ = false;
            if (!isDynamicIsland_) {
                ConfigManager::Get().posX = posX_;
                ConfigManager::Get().posY = posY_;
                ConfigManager::Save();
            }
            LONG ex = GetWindowLong(hwnd_, GWL_EXSTYLE);
            if (!(ex & WS_EX_TRANSPARENT)) {
                SetWindowLong(hwnd_, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
            }
        }
    }

    // 2. Right Shift hotkey over pig -> Open Dota-style settings
    static bool lastRShiftState = false;
    bool rshiftDown = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
    if (mouseOver && rshiftDown && !lastRShiftState) {
        DotaSettingsOverlay::Instance().Show(true);
    }
    lastRShiftState = rshiftDown;

    // 2b. Global Quick Advice Hotkeys: F5 = Поболтать, F6 = "Что купить?", F7 = "Что качать?"
    static bool lastF5State = false;
    bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (f5Down && !lastF5State) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetFriendlyChat());
    }
    lastF5State = f5Down;

    static bool lastF6State = false;
    bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    if (f6Down && !lastF6State) {
        VoiceAssistant::Instance().AskAdvice(0); // Buy advice
    }
    lastF6State = f6Down;

    static bool lastF7State = false;
    bool f7Down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    if (f7Down && !lastF7State) {
        VoiceAssistant::Instance().AskAdvice(1); // Skill advice
    }
    lastF7State = f7Down;

    static bool lastF8State = false;
    bool f8Down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    if (f8Down && !lastF8State) {
        OverlayWindow::Instance().Show(true);
        OverlayWindow::Instance().TriggerAdvice(HeroAdvisor::Instance().GetJoke()); // Joke
    }
    lastF8State = f8Down;

    // 3. Periodic Topmost & Dota visibility enforcement
    static int pollCounter = 0;
    if (++pollCounter >= 25) { // every ~400ms
        pollCounter = 0;
        if (hwnd_ && visible_ && !isDragging_) {
            SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }

        if (ConfigManager::Get().hideWhenNoDota && state_ == PigAnimationState::IDLE) {
            bool dotaRunning = DotaGsiService::Instance().IsDotaProcessRunning();
            if (visible_ && !dotaRunning) {
                Show(false);
            } else if (!visible_ && dotaRunning) {
                Show(true);
            }
        }
    }

    // 4. Low HP Sound repeat & Animation states
    if (state_ == PigAnimationState::PANIC_LOW_HP) {
        lastLowHpSoundTime_ += dt;
        if (lastLowHpSoundTime_ >= 2.5) {
            lastLowHpSoundTime_ = 0.0;
            PlayAudio(L"low_hp.wav");
            int idx = rand() % lowHpPhrases_.size();
            bubbleText_ = lowHpPhrases_[idx];
        }
    }
    else if (state_ != PigAnimationState::IDLE) {
        if (animTimer_ >= stateDuration_) {
            state_ = PigAnimationState::IDLE;
            animTimer_ = 0.0;
            bubbleText_ = L"";

            // If test animation ended and Dota is closed, hide immediately
            if (ConfigManager::Get().hideWhenNoDota && !DotaGsiService::Instance().IsDotaProcessRunning()) {
                Show(false);
            }
        }
    }

    Render();
}

static void AddRoundedRect(GraphicsPath& path, RectF rect, float radius) {
    float diameter = radius * 2.0f;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter, diameter, diameter, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
}

void OverlayWindow::Render() {
    if (!hwnd_ || !visible_) return;

    Bitmap bmp(winWidth_, winHeight_, PixelFormat32bppPARGB);
    Graphics g(&bmp);

    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));

    // Calculate Pig animation transforms
    int baseSize = ConfigManager::Get().pigSize;
    float pigW = (float)baseSize;
    float pigH = (float)baseSize;

    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float bubbleAlpha = 0.0f;

    if (state_ == PigAnimationState::IDLE) {
        offsetY = (float)(sin(animTimer_ * 2.5) * 4.0);
    }
    else if (state_ == PigAnimationState::JUMP_KILL) {
        double decay = exp(-animTimer_ * 0.45);
        double bounce = fabs(sin(animTimer_ * 7.5)) * decay;
        offsetY = (float)(-bounce * 75.0);

        scaleX = 1.0f + (float)(0.12 * sin(animTimer_ * 15.0) * decay);
        scaleY = 1.0f - (float)(0.10 * sin(animTimer_ * 15.0) * decay);

        // Bubble fade in and out
        if (animTimer_ < 0.3) {
            bubbleAlpha = (float)(animTimer_ / 0.3);
        } else if (animTimer_ > stateDuration_ - 0.5) {
            bubbleAlpha = (float)((stateDuration_ - animTimer_) / 0.5);
        } else {
            bubbleAlpha = 1.0f;
        }
    }
    else if (state_ == PigAnimationState::LOTUS_REMINDER) {
        double bounce = fabs(sin(animTimer_ * 6.0));
        offsetY = (float)(-bounce * 45.0);
        scaleX = 1.0f + (float)(0.08 * sin(animTimer_ * 12.0));
        scaleY = 1.0f - (float)(0.08 * sin(animTimer_ * 12.0));

        if (animTimer_ < 0.3) {
            bubbleAlpha = (float)(animTimer_ / 0.3);
        } else if (animTimer_ > stateDuration_ - 0.5) {
            bubbleAlpha = (float)((stateDuration_ - animTimer_) / 0.5);
        } else {
            bubbleAlpha = 1.0f;
        }
    }
    else if (state_ == PigAnimationState::SAD_DEATH) {
        double decay = exp(-animTimer_ * 0.5);
        offsetX = (float)(sin(animTimer_ * 20.0) * 10.0 * decay);
        offsetY = 14.0f;

        if (animTimer_ < 0.3) {
            bubbleAlpha = (float)(animTimer_ / 0.3);
        } else if (animTimer_ > stateDuration_ - 0.5) {
            bubbleAlpha = (float)((stateDuration_ - animTimer_) / 0.5);
        } else {
            bubbleAlpha = 1.0f;
        }
    }
    else if (state_ == PigAnimationState::PANIC_LOW_HP) {
        offsetX = (float)(sin(animTimer_ * 28.0) * 7.0);
        offsetY = (float)(sin(animTimer_ * 35.0) * 4.0);
        scaleX = 1.0f + (float)(sin(animTimer_ * 20.0) * 0.08);
        scaleY = 1.0f - (float)(sin(animTimer_ * 20.0) * 0.08);
        bubbleAlpha = 1.0f;
    }

    float drawW = pigW * scaleX;
    float drawH = pigH * scaleY;
    float pigX = (winWidth_ - drawW) / 2.0f + offsetX;
    float pigY = (float)winHeight_ - drawH - 12.0f + offsetY;

    if (isDynamicIsland_) {
        float iw = islandCurrentWidth_;
        float ih = islandCurrentHeight_;
        float ix = (winWidth_ - iw) / 2.0f;
        float iy = 4.0f;

        // Dynamic Island pill path
        GraphicsPath islandPath;
        AddRoundedRect(islandPath, RectF(ix, iy, iw, ih), ih * 0.5f);

        // Soft drop shadow
        GraphicsPath shadowPath;
        AddRoundedRect(shadowPath, RectF(ix - 3.0f, iy + 2.0f, iw + 6.0f, ih + 4.0f), (ih + 4.0f) * 0.5f);
        SolidBrush shadowBrush(Color(75, 0, 0, 0));
        g.FillPath(&shadowBrush, &shadowPath);

        // Dark background pill (#0c0c0e)
        SolidBrush bgBrush(Color(250, 12, 12, 15));
        g.FillPath(&bgBrush, &islandPath);

        // Sleek border
        Pen borderPen(Color(100, 255, 255, 255), 1.2f);
        g.DrawPath(&borderPen, &islandPath);

        // Mini Pig Icon
        float iconSize = ih - 12.0f;
        float iconX = ix + 10.0f;
        float iconY = iy + 6.0f;
        if (currentPigImage_) {
            g.DrawImage(currentPigImage_.get(), RectF(iconX, iconY, iconSize, iconSize));
        }

        if (!bubbleText_.empty() && bubbleAlpha > 0.05f) {
            // Speech text inside island
            Gdiplus::Font textFont(L"Segoe UI", 10.0f, FontStyleBold, UnitPoint);
            SolidBrush textBrush(Color((BYTE)(bubbleAlpha * 255.0f), 245, 245, 245));
            StringFormat sf;
            sf.SetAlignment(StringAlignmentNear);
            sf.SetLineAlignment(StringAlignmentCenter);
            sf.SetTrimming(StringTrimmingEllipsisCharacter);

            RectF textRect(iconX + iconSize + 10.0f, iy, iw - (iconSize + 22.0f), ih);
            g.DrawString(bubbleText_.c_str(), -1, &textFont, textRect, &sf, &textBrush);
        } else {
            // Idle Compact Island mode (matching Image 3!)
            float badgeX = iconX + iconSize + 8.0f;
            float badgeY = iy + (ih - 18.0f) * 0.5f;
            float badgeW = 28.0f;
            float badgeH = 18.0f;
            GraphicsPath badgePath;
            AddRoundedRect(badgePath, RectF(badgeX, badgeY, badgeW, badgeH), 4.0f);
            SolidBrush badgeBg(Color(255, 42, 44, 48));
            g.FillPath(&badgeBg, &badgePath);
            Gdiplus::Font badgeFont(L"Segoe UI", 8.0f, FontStyleBold, UnitPoint);
            SolidBrush badgeText(Color(255, 220, 225, 230));
            StringFormat bsf;
            bsf.SetAlignment(StringAlignmentCenter);
            bsf.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(L"RU", -1, &badgeFont, RectF(badgeX, badgeY, badgeW, badgeH), &bsf, &badgeText);

            SYSTEMTIME st;
            GetLocalTime(&st);
            wchar_t timeBuf[32];
            swprintf_s(timeBuf, L"%02d:%02d", st.wHour, st.wMinute);

            Gdiplus::Font timeFont(L"Segoe UI", 10.5f, FontStyleBold, UnitPoint);
            SolidBrush timeBrush(Color(255, 255, 255, 255));
            StringFormat tsf;
            tsf.SetAlignment(StringAlignmentNear);
            tsf.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(timeBuf, -1, &timeFont, PointF(badgeX + badgeW + 8.0f, iy + (ih - 18.0f) * 0.5f - 1.0f), &tsf, &timeBrush);

            bool dotaRunning = DotaGsiService::Instance().IsDotaProcessRunning();
            std::wstring statusStr = dotaRunning ? L"🎮 Dota 2" : L"☁ -9°";
            Gdiplus::Font statFont(L"Segoe UI", 9.0f, FontStyleRegular, UnitPoint);
            SolidBrush statBrush(Color(255, 175, 185, 195));
            g.DrawString(statusStr.c_str(), -1, &statFont, PointF(badgeX + badgeW + 52.0f, iy + (ih - 18.0f) * 0.5f), &tsf, &statBrush);
        }
    } else {
        // Draw 3D soft shadow if enabled
        if (ConfigManager::Get().enablePigShadow) {
            float shadowCenterY = (float)winHeight_ - 14.0f;
            float shadowCenterX = (float)winWidth_ / 2.0f + offsetX;
            float jumpRatio = std::max(0.0f, std::min(1.0f, -offsetY / 45.0f));
            float shadowW = drawW * 0.78f * (1.0f - jumpRatio * 0.35f);
            float shadowH = 18.0f * (1.0f - jumpRatio * 0.45f);
            BYTE shadowAlpha = (BYTE)(85.0f * (1.0f - jumpRatio * 0.5f));

            GraphicsPath shadowPath;
            shadowPath.AddEllipse(shadowCenterX - shadowW * 0.5f, shadowCenterY - shadowH * 0.5f, shadowW, shadowH);
            SolidBrush shadowBrush(Color(shadowAlpha, 0, 0, 0));
            g.FillPath(&shadowBrush, &shadowPath);
        }

        // Draw Pig Image
        Image* pDrawPig = currentPigImage_.get();
        if (state_ == PigAnimationState::PANIC_LOW_HP && ConfigManager::Get().enableBeatenPig && beatenPigImage_) {
            pDrawPig = beatenPigImage_.get();
        }
        if (pDrawPig) {
            g.DrawImage(pDrawPig, RectF(pigX, pigY, drawW, drawH));
        }

        // Draw dynamic auto-sizing speech bubble (Image 2 fix)
        if (bubbleAlpha > 0.01f && !bubbleText_.empty()) {
            BYTE a = (BYTE)(bubbleAlpha * 255.0f);

            std::unique_ptr<Gdiplus::Font> pFont;
            if (!customFontName_.empty() && fontCollection_) {
                pFont = std::make_unique<Gdiplus::Font>(customFontName_.c_str(), 11.5f, FontStyleBold, UnitPoint, fontCollection_.get());
            }
            if (!pFont || pFont->GetLastStatus() != Ok) {
                pFont = std::make_unique<Gdiplus::Font>(L"Segoe UI", 11.5f, FontStyleBold, UnitPoint);
            }

            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);

            float maxTextW = (float)winWidth_ - 36.0f;
            RectF layoutRect(0, 0, maxTextW, 800.0f);
            RectF measuredRect;
            g.MeasureString(bubbleText_.c_str(), -1, pFont.get(), layoutRect, &sf, &measuredRect);

            float bw = std::max(180.0f, std::min((float)winWidth_ - 24.0f, measuredRect.Width + 34.0f));
            float bh = std::max(56.0f, measuredRect.Height + 24.0f);
            float bx = (winWidth_ - bw) / 2.0f;
            float by = pigY - bh - 16.0f;
            if (by < 6.0f) by = 6.0f; // Clamp inside canvas, never cut off!

            GraphicsPath bubblePath;
            AddRoundedRect(bubblePath, RectF(bx, by, bw, bh), 16.0f);

            // Tail pointing down to pig head
            PointF tailPts[] = {
                PointF(bx + bw * 0.5f - 10.0f, by + bh - 1.0f),
                PointF(bx + bw * 0.5f, by + bh + 14.0f),
                PointF(bx + bw * 0.5f + 10.0f, by + bh - 1.0f)
            };
            bubblePath.AddPolygon(tailPts, 3);

            SolidBrush fillBrush(Color(a, 255, 255, 255));
            g.FillPath(&fillBrush, &bubblePath);

            Pen borderPen(Color(a, 35, 35, 35), 2.5f);
            g.DrawPath(&borderPen, &bubblePath);

            SolidBrush textBrush(Color(a, 20, 20, 20));
            RectF textRect(bx + 8.0f, by + 6.0f, bw - 16.0f, bh - 12.0f);
            g.DrawString(bubbleText_.c_str(), -1, pFont.get(), textRect, &sf, &textBrush);
        }
    }

    // Transfer to Layered Window
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = NULL;
    bmp.GetHBITMAP(Color(0, 0, 0, 0), &hBitmap);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBitmap);

    POINT ptDst = { posX_, posY_ };
    POINT ptSrc = { 0, 0 };
    SIZE sizeWnd = { winWidth_, winHeight_ };

    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd_, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}
