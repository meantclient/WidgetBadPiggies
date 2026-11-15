#include "dota_gsi.h"
#include "config.h"
#include "hero_advisor.h"
#include "overlay.h"
#include <tlhelp32.h>
#include <fstream>
#include <sstream>
#include <cctype>
#include <nlohmann/json.hpp>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")

using json = nlohmann::json;

DotaGsiService& DotaGsiService::Instance() {
    static DotaGsiService instance;
    return instance;
}

DotaGsiService::DotaGsiService() {}

DotaGsiService::~DotaGsiService() {
    Stop();
}

bool DotaGsiService::Start(int port) {
    if (running_.load()) return true;
    running_.store(true);

    // Initial config check
    std::wstring msg;
    SetupDotaConfig(msg);

    // Start background threads
    serverThread_ = std::thread(&DotaGsiService::ServerThread, this, port);
    monitorThread_ = std::thread(&DotaGsiService::MonitorThread, this);
    return true;
}

void DotaGsiService::Stop() {
    if (!running_.load()) return;
    running_.store(false);

    // Wake up sockets if blocked by connecting to local port
    SOCKET wakeSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (wakeSock != INVALID_SOCKET) {
        sockaddr_in addr = { 0 };
        addr.sin_family = AF_INET;
        addr.sin_port = htons(4000);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        connect(wakeSock, (sockaddr*)&addr, sizeof(addr));
        closesocket(wakeSock);
    }

    if (serverThread_.joinable()) serverThread_.join();
    if (monitorThread_.joinable()) monitorThread_.join();
}

bool DotaGsiService::IsDotaProcessRunning() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { 0 };
    pe.dwSize = sizeof(pe);
    bool found = false;

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"dota2.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return found;
}

void DotaGsiService::MonitorThread() {
    bool lastState = !IsDotaProcessRunning();
    while (running_.load()) {
        bool current = IsDotaProcessRunning();
        if (current != lastState) {
            lastState = current;
            dotaRunning_.store(current);
            if (onDotaState_) {
                onDotaState_(current);
            }
        }
        for (int i = 0; i < 5 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void DotaGsiService::ServerThread(int port) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    int opt = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(listenSock, 10) == SOCKET_ERROR) {
        closesocket(listenSock);
        WSACleanup();
        return;
    }

    while (running_.load()) {
        SOCKET clientSock = accept(listenSock, NULL, NULL);
        if (clientSock == INVALID_SOCKET) break;
        if (!running_.load()) {
            closesocket(clientSock);
            break;
        }

        // Set socket receive timeout 1 second
        DWORD timeoutMs = 1000;
        setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

        std::string req;
        char buffer[4096];
        int bytes = 0;
        int contentLength = -1;
        size_t headerEnd = std::string::npos;

        while (true) {
            bytes = recv(clientSock, buffer, sizeof(buffer), 0);
            if (bytes <= 0) break;
            req.append(buffer, bytes);

            if (headerEnd == std::string::npos) {
                headerEnd = req.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    std::string lowerReq = req.substr(0, headerEnd);
                    for (auto& c : lowerReq) c = (char)tolower(c);
                    size_t clPos = lowerReq.find("content-length:");
                    if (clPos != std::string::npos) {
                        contentLength = std::atoi(lowerReq.c_str() + clPos + 15);
                    } else {
                        break;
                    }
                }
            }

            if (headerEnd != std::string::npos && contentLength >= 0) {
                if ((int)(req.length() - (headerEnd + 4)) >= contentLength) {
                    break;
                }
            }
        }

        // Send standard 200 OK
        const char* resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(clientSock, resp, (int)strlen(resp), 0);
        closesocket(clientSock);

        if (headerEnd != std::string::npos) {
            std::string body = req.substr(headerEnd + 4);
            ProcessGsiPayload(body);
        }
    }

    closesocket(listenSock);
    WSACleanup();
}

void DotaGsiService::ProcessGsiPayload(const std::string& jsonStr) {
    // Log first 30 packets for easy verification
    static int logCount = 0;
    if (logCount < 30) {
        std::wstring logPath = ConfigManager::GetExeDir() + L"\\gsi_log.txt";
        std::ofstream logFile(logPath, std::ios::app);
        if (logFile.is_open()) {
            logFile << "=== GSI PACKET " << ++logCount << " ===\n" << jsonStr << "\n\n";
        }
    }

    try {
        json root = json::parse(jsonStr);

        // 1. Process Player Stats
        if (root.contains("player") && root["player"].is_object()) {
            const auto& player = root["player"];

            int kills = -1;
            if (player.contains("kills")) {
                if (player["kills"].is_number()) {
                    kills = player["kills"].get<int>();
                } else if (player["kills"].is_string()) {
                    kills = std::atoi(player["kills"].get<std::string>().c_str());
                }
            }

            int deaths = -1;
            if (player.contains("deaths")) {
                if (player["deaths"].is_number()) {
                    deaths = player["deaths"].get<int>();
                } else if (player["deaths"].is_string()) {
                    deaths = std::atoi(player["deaths"].get<std::string>().c_str());
                }
            }

            int streak = 0;
            if (player.contains("kill_streak")) {
                if (player["kill_streak"].is_number()) {
                    streak = player["kill_streak"].get<int>();
                } else if (player["kill_streak"].is_string()) {
                    streak = std::atoi(player["kill_streak"].get<std::string>().c_str());
                }
            }

            if (kills >= 0) {
                if (lastKills_ >= 0 && kills > lastKills_) {
                    if (onKill_) {
                        onKill_(kills, streak);
                    }
                }
                lastKills_ = kills;
            }

            if (deaths >= 0) {
                if (lastDeaths_ >= 0 && deaths > lastDeaths_) {
                    if (onDeath_) {
                        onDeath_();
                    }
                }
                lastDeaths_ = deaths;
            }
        }

        // 2. Process Hero Stats (Low HP & Vignette)
        if (root.contains("hero") && root["hero"].is_object()) {
            const auto& hero = root["hero"];
            int hpPercent = -1;
            if (hero.contains("health_percent")) {
                if (hero["health_percent"].is_number()) {
                    hpPercent = hero["health_percent"].get<int>();
                } else if (hero["health_percent"].is_string()) {
                    hpPercent = std::atoi(hero["health_percent"].get<std::string>().c_str());
                }
            }
            bool alive = true;
            if (hero.contains("alive")) {
                if (hero["alive"].is_boolean()) {
                    alive = hero["alive"].get<bool>();
                }
            }

            if (hpPercent >= 0 && alive) {
                bool isLow = (hpPercent <= 25);
                if (onLowHp_) onLowHp_(isLow, hpPercent);
            } else if (!alive) {
                if (onLowHp_) onLowHp_(false, 0);
            }
        }

        // 3. Process Map / Match Reset & Lotus Pool Timing
        if (root.contains("map") && root["map"].is_object()) {
            const auto& mapObj = root["map"];
            if (mapObj.contains("matchid")) {
                std::string matchId;
                if (mapObj["matchid"].is_string()) {
                    matchId = mapObj["matchid"].get<std::string>();
                } else if (mapObj["matchid"].is_number()) {
                    matchId = std::to_string(mapObj["matchid"].get<uint64_t>());
                }

                if (!matchId.empty() && matchId != "0" && matchId != lastMatchId_) {
                    lastMatchId_ = matchId;
                    lastKills_ = -1;
                    lastDeaths_ = -1;
                    lastLotusIndex_ = -1;
                    lastLotusPreCycle_ = -1;
                    lastLotusSpawnCycle_ = -1;
                    HeroAdvisor::Instance().Reset();
                }
            }

            // Lotus pool check:
            // Normal mode: spawns every 3 minutes (180s) starting from 3:00 (180s)
            // Turbo mode: spawns every 1.5 minutes (90s) starting from 1:30 (90s)
            if (mapObj.contains("clock_time")) {
                int clockTime = -9999;
                if (mapObj["clock_time"].is_number()) {
                    clockTime = mapObj["clock_time"].get<int>();
                } else if (mapObj["clock_time"].is_string()) {
                    clockTime = std::atoi(mapObj["clock_time"].get<std::string>().c_str());
                }

                // Detect Turbo Mode
                bool isTurbo = false;
                int userMode = ConfigManager::Get().lotusMode;
                if (userMode == 2) {
                    isTurbo = true; // Forced Turbo
                } else if (userMode == 1) {
                    isTurbo = false; // Forced Normal
                } else {
                    // Auto-detection
                    if (mapObj.contains("game_mode")) {
                        if (mapObj["game_mode"].is_number() && mapObj["game_mode"].get<int>() == 23) {
                            isTurbo = true;
                        } else if (mapObj["game_mode"].is_string()) {
                            std::string gm = mapObj["game_mode"].get<std::string>();
                            for (auto& c : gm) c = (char)tolower(c);
                            if (gm.find("turbo") != std::string::npos) isTurbo = true;
                        }
                    }
                    if (!isTurbo && mapObj.contains("name") && mapObj["name"].is_string()) {
                        std::string nm = mapObj["name"].get<std::string>();
                        for (auto& c : nm) c = (char)tolower(c);
                        if (nm.find("turbo") != std::string::npos) isTurbo = true;
                    }
                    if (!isTurbo && mapObj.contains("customgamename") && mapObj["customgamename"].is_string()) {
                        std::string cm = mapObj["customgamename"].get<std::string>();
                        for (auto& c : cm) c = (char)tolower(c);
                        if (cm.find("turbo") != std::string::npos) isTurbo = true;
                    }
                }

                int interval = isTurbo ? 90 : 180;
                int preWarnSec = isTurbo ? 10 : 15;

                if (clockTime >= 0) {
                    int cycle = clockTime / interval;
                    int rem = clockTime % interval;

                    // 1. Advance warning before spawn
                    int warnStart = interval - preWarnSec;
                    int warnEnd = interval - 2;
                    if (rem >= warnStart && rem <= warnEnd) {
                        if (lastLotusPreCycle_ != cycle) {
                            lastLotusPreCycle_ = cycle;
                            if (onLotusPre_) {
                                onLotusPre_(isTurbo);
                            }
                        }
                    }

                    // 2. Exact spawn notification (for cycle >= 1)
                    if (clockTime >= interval && rem >= 0 && rem <= 12) {
                        if (lastLotusSpawnCycle_ != cycle) {
                            lastLotusSpawnCycle_ = cycle;
                            if (onLotus_) {
                                onLotus_(isTurbo);
                            }
                        }
                    }
                }
            }
        }

        // 4. Update HeroAdvisor (Dota2ProTracker Guide & Proactive Assistant)
        std::string heroName = "";
        int heroLevel = 1;
        int xpos = 0;
        int ypos = 0;
        if (root.contains("hero") && root["hero"].is_object()) {
            const auto& h = root["hero"];
            if (h.contains("name") && h["name"].is_string()) heroName = h["name"].get<std::string>();
            if (h.contains("level") && h["level"].is_number()) heroLevel = h["level"].get<int>();
            if (h.contains("xpos") && h["xpos"].is_number()) xpos = h["xpos"].get<int>();
            if (h.contains("ypos") && h["ypos"].is_number()) ypos = h["ypos"].get<int>();
        }

        int gold = 0;
        if (root.contains("player") && root["player"].is_object()) {
            const auto& p = root["player"];
            if (p.contains("gold") && p["gold"].is_number()) gold = p["gold"].get<int>();
        }

        int cTime = 0;
        if (root.contains("map") && root["map"].is_object() && root["map"].contains("clock_time")) {
            if (root["map"]["clock_time"].is_number()) cTime = root["map"]["clock_time"].get<int>();
            else if (root["map"]["clock_time"].is_string()) cTime = std::atoi(root["map"]["clock_time"].get<std::string>().c_str());
        }

        std::vector<std::string> currentItems;
        if (root.contains("items") && root["items"].is_object()) {
            const auto& itm = root["items"];
            for (int i = 0; i < 6; ++i) {
                std::string slotKey = "slot" + std::to_string(i);
                if (itm.contains(slotKey) && itm[slotKey].is_object() && itm[slotKey].contains("name")) {
                    currentItems.push_back(itm[slotKey]["name"].get<std::string>());
                }
            }
        }

        std::vector<std::pair<std::string, int>> abilities;
        if (root.contains("abilities") && root["abilities"].is_object()) {
            const auto& ab = root["abilities"];
            for (int i = 0; i < 8; ++i) {
                std::string abKey = "ability" + std::to_string(i);
                if (ab.contains(abKey) && ab[abKey].is_object()) {
                    std::string aName = ab[abKey].value("name", "");
                    int aLvl = ab[abKey].value("level", 0);
                    if (!aName.empty()) abilities.push_back({ aName, aLvl });
                }
            }
        }

        if (!heroName.empty()) {
            HeroAdvisor::Instance().UpdateGameState(heroName, gold, heroLevel, xpos, ypos, cTime, currentItems, abilities);

            if (ConfigManager::Get().enableProactiveAdvisor) {
                std::wstring proactiveAdvice;
                if (HeroAdvisor::Instance().CheckProactiveAdvice(proactiveAdvice)) {
                    OverlayWindow::Instance().Show(true);
                    OverlayWindow::Instance().TriggerAdvice(proactiveAdvice);
                }
            }
        }
    } catch (...) {
        // Ignore parse errors
    }
}

static std::wstring FindDotaCfgPath() {
    HKEY hKey;
    wchar_t steamPath[MAX_PATH] = { 0 };
    DWORD size = sizeof(steamPath);

    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"SteamPath", NULL, NULL, (LPBYTE)steamPath, &size);
        RegCloseKey(hKey);
    }

    if (wcslen(steamPath) == 0) return L"";

    // Standard path
    std::wstring dotaCfg = std::wstring(steamPath) + L"\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg";
    for (auto& ch : dotaCfg) if (ch == L'/') ch = L'\\';

    if (PathFileExistsW(dotaCfg.c_str())) {
        return dotaCfg;
    }

    // Try reading libraryfolders.vdf
    std::wstring libVdf = std::wstring(steamPath) + L"\\steamapps\\libraryfolders.vdf";
    for (auto& ch : libVdf) if (ch == L'/') ch = L'\\';

    std::wifstream vdfFile(libVdf);
    if (vdfFile.is_open()) {
        std::wstring line;
        while (std::getline(vdfFile, line)) {
            size_t p = line.find(L"\"path\"");
            if (p != std::wstring::npos) {
                size_t firstQuote = line.find(L"\"", p + 6);
                if (firstQuote != std::wstring::npos) {
                    size_t secondQuote = line.find(L"\"", firstQuote + 1);
                    if (secondQuote != std::wstring::npos) {
                        std::wstring libPath = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                        std::wstring cleanPath;
                        for (size_t i = 0; i < libPath.length(); ++i) {
                            if (libPath[i] == L'\\' && i + 1 < libPath.length() && libPath[i + 1] == L'\\') {
                                continue;
                            }
                            cleanPath += libPath[i];
                        }
                        std::wstring checkDota = cleanPath + L"\\steamapps\\common\\dota 2 beta\\game\\dota\\cfg";
                        if (PathFileExistsW(checkDota.c_str())) {
                            return checkDota;
                        }
                    }
                }
            }
        }
    }

    return L"";
}

bool DotaGsiService::SetupDotaConfig(std::wstring& outMsg) {
    std::wstring cfgDir = FindDotaCfgPath();
    if (cfgDir.empty()) {
        outMsg = L"Папка Dota 2 не найдена. Убедитесь, что Steam и Dota 2 установлены.";
        return false;
    }

    std::string content =
        "\"Dota 2 Integration Configuration\"\n"
        "{\n"
        "    \"uri\"           \"http://127.0.0.1:4000/\"\n"
        "    \"timeout\"       \"5.0\"\n"
        "    \"buffer\"        \"0.1\"\n"
        "    \"throttle\"      \"0.1\"\n"
        "    \"heartbeat\"     \"30.0\"\n"
        "    \"data\"\n"
        "    {\n"
        "        \"provider\"      \"1\"\n"
        "        \"map\"           \"1\"\n"
        "        \"player\"        \"1\"\n"
        "        \"hero\"          \"1\"\n"
        "        \"abilities\"     \"1\"\n"
        "        \"items\"         \"1\"\n"
        "        \"buildings\"     \"1\"\n"
        "        \"draft\"         \"1\"\n"
        "        \"wearables\"     \"1\"\n"
        "    }\n"
        "}\n";

    // Create gamestate_integration subfolder in game/dota/cfg
    std::wstring gsiSubDir = cfgDir + L"\\gamestate_integration";
    CreateDirectoryW(gsiSubDir.c_str(), NULL);

    std::wstring cfgFile1 = gsiSubDir + L"\\gamestate_integration_badpigs.cfg";
    std::ofstream out1(cfgFile1, std::ios::trunc);
    if (out1.is_open()) { out1 << content; out1.close(); }

    std::wstring cfgFile2 = cfgDir + L"\\gamestate_integration_badpigs.cfg";
    std::ofstream out2(cfgFile2, std::ios::trunc);
    if (out2.is_open()) { out2 << content; out2.close(); }

    // Also write to game/core/cfg if it exists
    size_t pos = cfgDir.rfind(L"\\dota\\cfg");
    if (pos != std::wstring::npos) {
        std::wstring coreDir = cfgDir.substr(0, pos) + L"\\core\\cfg";
        std::wstring coreGsiSubDir = coreDir + L"\\gamestate_integration";
        CreateDirectoryW(coreGsiSubDir.c_str(), NULL);

        std::wstring coreCfg1 = coreGsiSubDir + L"\\gamestate_integration_badpigs.cfg";
        std::ofstream outC1(coreCfg1, std::ios::trunc);
        if (outC1.is_open()) { outC1 << content; outC1.close(); }

        std::wstring coreCfg2 = coreDir + L"\\gamestate_integration_badpigs.cfg";
        std::ofstream outC2(coreCfg2, std::ios::trunc);
        if (outC2.is_open()) { outC2 << content; outC2.close(); }
    }

    outMsg = L"Файл интеграции успешно создан:\n" + cfgFile1;
    return true;
}
