#pragma once
#include "common.h"
#include <functional>
#include <atomic>
#include <thread>

class DotaGsiService {
public:
    using KillCallback = std::function<void(int kills, int streak)>;
    using DeathCallback = std::function<void()>;
    using DotaStateCallback = std::function<void(bool isRunning)>;
    using LowHpCallback = std::function<void(bool isLow, int hpPercent)>;
    using LotusCallback = std::function<void(bool isTurbo)>;

    static DotaGsiService& Instance();

    bool Start(int port = 4000);
    void Stop();

    void SetOnKill(KillCallback cb) { onKill_ = cb; }
    void SetOnDeath(DeathCallback cb) { onDeath_ = cb; }
    void SetOnDotaState(DotaStateCallback cb) { onDotaState_ = cb; }
    void SetOnLowHp(LowHpCallback cb) { onLowHp_ = cb; }
    void SetOnLotus(LotusCallback cb) { onLotus_ = cb; }
    void SetOnLotusPre(LotusCallback cb) { onLotusPre_ = cb; }

    bool SetupDotaConfig(std::wstring& outMsg);
    bool IsDotaProcessRunning();
    bool IsDotaActive() const { return dotaRunning_.load(); }

private:
    DotaGsiService();
    ~DotaGsiService();

    void ServerThread(int port);
    void MonitorThread();
    void ProcessGsiPayload(const std::string& jsonStr);

    std::atomic<bool> running_{ false };
    std::atomic<bool> dotaRunning_{ false };
    std::thread serverThread_;
    std::thread monitorThread_;

    KillCallback onKill_;
    DeathCallback onDeath_;
    DotaStateCallback onDotaState_;
    LowHpCallback onLowHp_;
    LotusCallback onLotus_;
    LotusCallback onLotusPre_;

    // Tracking state
    int lastKills_ = -1;
    int lastDeaths_ = -1;
    int lastLotusIndex_ = -1;
    int lastLotusPreCycle_ = -1;
    int lastLotusSpawnCycle_ = -1;
    std::string lastMatchId_ = "";
};
