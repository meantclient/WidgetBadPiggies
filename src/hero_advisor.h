#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <map>

struct BuildItem {
    std::wstring nameRu;
    int cost;
    std::string idSub;                    // GSI item identifier substring
    std::vector<std::string> altSubs;     // upgraded or alternate forms
    std::wstring roleReason;              // Why to buy
};

struct HeroBuildMeta {
    std::wstring heroDisplayName;
    std::wstring primaryRole;             // "Мид", "Керри", "Оффлейн", "Саппорт"
    std::wstring laneType;                // Recommended lane
    std::wstring earlyItemsRu;
    std::vector<BuildItem> progression;   // Step-by-step item build chain
    std::wstring skillPriority;
    std::wstring laneStrategy;
};

class HeroAdvisor {
public:
    static HeroAdvisor& Instance();

    void UpdateGameState(
        const std::string& heroName,
        int gold,
        int heroLevel,
        int xpos,
        int ypos,
        int clockTime,
        const std::vector<std::string>& currentItems,
        const std::vector<std::pair<std::string, int>>& abilities
    );

    // Queries from user (voice or hotkey F5/F6/F7)
    std::wstring GetBuyAdvice();
    std::wstring GetSkillAdvice();
    std::wstring GetGeneralAdvice();
    std::wstring GetFriendlyChat();
    std::wstring GetJoke();
    std::wstring GetHowAreYou();
    std::wstring GetThanksReaction();
    std::wstring GetCheerUp();
    std::wstring GetWakePrompt();

    // Event commentary
    std::wstring GetDeathRoast();
    std::wstring GetKillComment(int streak);
    std::wstring GetLotusComment(bool isTurbo);

    // Reset on new match
    void Reset();

    // Check if there is a proactive tip to speak
    bool CheckProactiveAdvice(std::wstring& outAdvice);

    std::string GetCurrentHeroName() const { return currentHeroName_; }
    std::wstring GetDetectedLane() const { return detectedLane_; }

    bool HasItem(const BuildItem& item);
    HeroBuildMeta FindHeroMeta(const std::string& heroName);

private:
    HeroAdvisor();
    ~HeroAdvisor();

    void InitHeroDatabase();
    std::wstring DetectLane(int x, int y);

    std::string currentHeroName_ = "";
    int currentGold_ = 0;
    int currentLevel_ = 0;
    int posX_ = 0;
    int posY_ = 0;
    int clockTime_ = 0;
    std::vector<std::string> currentItems_;
    std::vector<std::pair<std::string, int>> abilities_;

    std::wstring detectedLane_ = L"";
    bool laneAnnounced_ = false;
    std::string lastAffordableItem_ = "";
    int lastNotifiedLevel_ = 0;

    std::map<std::string, HeroBuildMeta> metaDatabase_;
};
