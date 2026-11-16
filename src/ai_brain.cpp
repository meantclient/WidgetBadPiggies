#include "ai_brain.h"
#include "config.h"
#include "local_ai.h"
#include "hero_advisor.h"
#include "voice_assistant.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <wininet.h>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <shlobj.h>

#pragma comment(lib, "wininet.lib")

// UTF-8 Helper functions using native Win32 API
static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (size <= 1) return "";
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, NULL, NULL);
    return result;
}

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    if (size <= 1) return L"";
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

// Win32 Byte File I/O (100% immune to Cyrillic username Kaldi/fopen crashes)
static bool ReadFileToString(const std::wstring& path, std::string& outData) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return false;
    }

    outData.resize(fileSize);
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hFile, &outData[0], fileSize, &bytesRead, NULL);
    CloseHandle(hFile);
    return (ok && bytesRead == fileSize);
}

static bool WriteStringToFile(const std::wstring& path, const std::string& data) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(hFile, data.data(), (DWORD)data.size(), &bytesWritten, NULL);
    CloseHandle(hFile);
    return (ok && bytesWritten == (DWORD)data.size());
}

static std::string UrlEncodeUtf8(const std::wstring& value) {
    std::string bytes = WideToUtf8(value), out;
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : bytes) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
        else if (c == ' ') out += "%20";
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out;
}

static bool HttpGetUtf8(const std::wstring& url, std::string& body) {
    HINTERNET session = InternetOpenW(L"BadPiggiesWidget/5.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!session) return false;
    DWORD timeout = 7000;
    InternetSetOptionW(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    HINTERNET request = InternetOpenUrlW(session, url.c_str(), nullptr, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (!request) { InternetCloseHandle(session); return false; }
    char buffer[4096]; DWORD got = 0;
    while (InternetReadFile(request, buffer, sizeof(buffer), &got) && got) {
        body.append(buffer, got);
        if (body.size() > 1024 * 1024) break;
    }
    InternetCloseHandle(request); InternetCloseHandle(session);
    return !body.empty();
}

static std::wstring WeatherCodeRu(int code) {
    if (code == 0) return L"ясно";
    if (code <= 3) return L"облачно";
    if (code == 45 || code == 48) return L"туман";
    if (code == 51 || code == 53 || code == 55 || code == 56 || code == 57) return L"морось";
    if (code == 61 || code == 63 || code == 65 || code == 80 || code == 81 || code == 82) return L"дождь";
    if (code == 66 || code == 67) return L"ледяной дождь";
    if (code == 71 || code == 73 || code == 75 || code == 77 || code == 85 || code == 86) return L"снег";
    if (code >= 95) return L"гроза";
    return L"переменная погода";
}

static bool TrySimpleArithmetic(const std::wstring& text, std::wstring& result) {
    std::vector<std::wstring> tokens;
    std::wstring token;
    for (wchar_t c : text) {
        if (iswalnum(c) || c == L'+' || c == L'-' || c == L'*' || c == L'/' || c == L'×' || c == L'÷') token += c;
        else if (!token.empty()) { tokens.push_back(token); token.clear(); }
    }
    if (!token.empty()) tokens.push_back(token);
    auto number = [](const std::wstring& word, double& value) {
        static const std::map<std::wstring, int> words = {
            {L"ноль",0},{L"один",1},{L"одна",1},{L"одно",1},{L"раз",1},{L"два",2},{L"две",2},{L"три",3},
            {L"четыре",4},{L"пять",5},{L"шесть",6},{L"семь",7},{L"восемь",8},{L"девять",9},{L"десять",10},
            {L"одиннадцать",11},{L"двенадцать",12},{L"тринадцать",13},{L"четырнадцать",14},{L"пятнадцать",15},
            {L"шестнадцать",16},{L"семнадцать",17},{L"восемнадцать",18},{L"девятнадцать",19},{L"двадцать",20}
        };
        auto found = words.find(word);
        if (found != words.end()) { value = found->second; return true; }
        wchar_t* end = nullptr;
        double parsed = wcstod(word.c_str(), &end);
        if (end && *end == L'\0' && end != word.c_str()) { value = parsed; return true; }
        return false;
    };
    auto op = [](const std::wstring& word) -> wchar_t {
        if (word == L"+" || word == L"плюс" || word == L"сложить") return L'+';
        if (word == L"-" || word == L"минус" || word == L"вычесть") return L'-';
        if (word == L"*" || word == L"×" || word == L"умножить" || word == L"умножитьна") return L'*';
        if (word == L"/" || word == L"÷" || word == L"разделить" || word == L"делить") return L'/';
        return 0;
    };
    double lhs = 0, rhs = 0;
    wchar_t operation = 0;
    int numbers = 0;
    for (const auto& t : tokens) {
        double value = 0;
        if (number(t, value)) {
            if (numbers == 0) lhs = value;
            else if (numbers == 1) rhs = value;
            ++numbers;
        } else if (!operation) {
            operation = op(t);
        }
    }
    if (numbers != 2 || !operation) return false;
    double answer = 0;
    if (operation == L'+') answer = lhs + rhs;
    else if (operation == L'-') answer = lhs - rhs;
    else if (operation == L'*') answer = lhs * rhs;
    else { if (rhs == 0) { result = L"На ноль делить нельзя."; return true; } answer = lhs / rhs; }
    std::wstring a = std::to_wstring(lhs), b = std::to_wstring(rhs), c = std::to_wstring(answer);
    auto clean = [](std::wstring& n) { while (n.find(L'.') != std::wstring::npos && !n.empty() && n.back() == L'0') n.pop_back(); if (!n.empty() && n.back() == L'.') n.pop_back(); };
    clean(a); clean(b); clean(c);
    result = a + L" " + std::wstring(1, operation) + L" " + b + L" = " + c + L".";
    return true;
}

static bool LaunchStartMenuShortcut(const std::wstring& appName) {
    std::wstring query = appName;
    while (!query.empty() && iswspace(query.front())) query.erase(query.begin());
    while (!query.empty() && iswspace(query.back())) query.pop_back();
    if (query.empty()) return false;
    for (auto& c : query) c = (wchar_t)towlower(c);

    wchar_t roots[2][MAX_PATH] = {};
    SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, SHGFP_TYPE_CURRENT, roots[0]);
    SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, SHGFP_TYPE_CURRENT, roots[1]);
    for (const auto& root : roots) {
        if (!root[0]) continue;
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (!it->is_regular_file(ec) || ec || it->path().extension() != L".lnk") continue;
            std::wstring label = it->path().stem().wstring();
            for (auto& c : label) c = (wchar_t)towlower(c);
            if (label.find(query) != std::wstring::npos || query.find(label) != std::wstring::npos) {
                auto result = (INT_PTR)ShellExecuteW(nullptr, L"open", it->path().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return result > 32;
            }
        }
    }
    return false;
}

// ============================================================================
// AiMemory Implementation (Universal Persistent Memory)
// ============================================================================

AiMemory& AiMemory::Instance() {
    static AiMemory inst;
    return inst;
}

AiMemory::AiMemory() {
    Load();
}

AiMemory::~AiMemory() {
    Save();
}

void AiMemory::Load() {
    std::wstring path = ConfigManager::GetExeDir() + L"\\assistant_memory.json";
    std::string jsonStr;
    if (!ReadFileToString(path, jsonStr)) {
        musicService_ = L"yandex";
        customPlaylistUrl_ = L"";
        return;
    }

    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (j.contains("music_service") && j["music_service"].is_string()) {
            musicService_ = Utf8ToWide(j["music_service"].get<std::string>());
        }
        if (j.contains("custom_playlist_url") && j["custom_playlist_url"].is_string()) {
            customPlaylistUrl_ = Utf8ToWide(j["custom_playlist_url"].get<std::string>());
        }
        if (j.contains("facts") && j["facts"].is_object()) {
            facts_.clear();
            for (auto& el : j["facts"].items()) {
                if (el.value().is_string()) {
                    facts_[Utf8ToWide(el.key())] = Utf8ToWide(el.value().get<std::string>());
                }
            }
        }
        if (j.contains("history") && j["history"].is_array()) {
            history_.clear();
            for (auto& item : j["history"]) {
                if (item.contains("user") && item.contains("pig")) {
                    DialogHistoryItem dh;
                    dh.userQuery = Utf8ToWide(item["user"].get<std::string>());
                    dh.pigResponse = Utf8ToWide(item["pig"].get<std::string>());
                    dh.timestamp = item.value("timestamp", (DWORD)0);
                    history_.push_back(dh);
                }
            }
        }
        if (j.contains("style") && j["style"].is_object()) {
            casualStyle_ = j["style"].value("casual", 0);
            conciseStyle_ = j["style"].value("concise", 0);
        }
    } catch (...) {}

    if (musicService_.empty()) musicService_ = L"yandex";
}

void AiMemory::Save() {
    std::wstring path = ConfigManager::GetExeDir() + L"\\assistant_memory.json";
    try {
        nlohmann::json j;
        j["music_service"] = WideToUtf8(musicService_);
        j["custom_playlist_url"] = WideToUtf8(customPlaylistUrl_);

        nlohmann::json factsObj = nlohmann::json::object();
        for (const auto& pair : facts_) {
            factsObj[WideToUtf8(pair.first)] = WideToUtf8(pair.second);
        }
        j["facts"] = factsObj;

        nlohmann::json histArr = nlohmann::json::array();
        int startIdx = (int)history_.size() > 20 ? (int)history_.size() - 20 : 0;
        for (size_t i = startIdx; i < history_.size(); ++i) {
            nlohmann::json hItem;
            hItem["user"] = WideToUtf8(history_[i].userQuery);
            hItem["pig"] = WideToUtf8(history_[i].pigResponse);
            hItem["timestamp"] = history_[i].timestamp;
            histArr.push_back(hItem);
        }
        j["history"] = histArr;
        j["style"] = { {"casual", casualStyle_}, {"concise", conciseStyle_} };

        std::string jsonStr = j.dump(2);
        WriteStringToFile(path, jsonStr);
    } catch (...) {}
}

void AiMemory::SetFact(const std::wstring& key, const std::wstring& value) {
    facts_[key] = value;
    Save();
}

std::wstring AiMemory::GetFact(const std::wstring& key) const {
    auto it = facts_.find(key);
    if (it != facts_.end()) return it->second;
    return L"";
}

std::wstring AiMemory::FindMatchingFact(const std::wstring& query) const {
    std::wstring q = query;
    for (auto& ch : q) ch = (wchar_t)towlower(ch);

    for (const auto& pair : facts_) {
        std::wstring k = pair.first;
        for (auto& ch : k) ch = (wchar_t)towlower(ch);
        std::wstring v = pair.second;
        for (auto& ch : v) ch = (wchar_t)towlower(ch);

        if (q.find(k) != std::wstring::npos || k.find(q) != std::wstring::npos) {
            return L"Ты просил запомнить: \"" + pair.second + L"\". Я всё помню, братан!";
        }
    }
    return L"";
}

std::map<std::wstring, std::wstring> AiMemory::GetAllFacts() const {
    return facts_;
}

void AiMemory::ClearMemory() {
    facts_.clear();
    history_.clear();
    Save();
}

std::wstring AiMemory::GetMemorySummary() const {
    std::wstring playerName = ConfigManager::Get().playerName;
    if (playerName.empty()) playerName = L"Братуха";

    if (facts_.empty()) {
        return L"Пока в моей памяти ничего не записано, " + playerName + L". Скажи мне что-нибудь вроде: 'Свин, запомни, что...' и я обязательно всё сохраню!";
    }

    std::wstring res = L"Вот что я помню, " + playerName + L": ";
    int count = 0;
    for (const auto& pair : facts_) {
        if (++count > 4) break;
        res += pair.second + L". ";
    }
    res += L"Я на чилле и ничего не забываю!";
    return res;
}

void AiMemory::SetMusicService(const std::wstring& service, const std::wstring& customUrl) {
    musicService_ = service;
    if (!customUrl.empty()) {
        customPlaylistUrl_ = customUrl;
    }
    Save();
}

std::wstring AiMemory::GetMusicService() const {
    return musicService_.empty() ? L"yandex" : musicService_;
}

std::wstring AiMemory::GetMusicServiceNameRu() const {
    if (musicService_ == L"spotify") return L"Спотифай";
    if (musicService_ == L"vk") return L"ВК Музыке";
    if (musicService_ == L"youtube") return L"Ютуб Музыке";
    return L"Яндекс Музыке";
}

std::wstring AiMemory::GetCustomPlaylistUrl() const {
    return customPlaylistUrl_;
}

void AiMemory::AddDialogHistory(const std::wstring& userQuery, const std::wstring& pigResponse) {
    DialogHistoryItem item;
    item.userQuery = userQuery;
    item.pigResponse = pigResponse;
    item.timestamp = GetTickCount();
    history_.push_back(item);
    if (history_.size() > 30) {
        history_.erase(history_.begin(), history_.begin() + 10);
    }
    Save();
}

std::vector<DialogHistoryItem> AiMemory::GetRecentHistory(int maxCount) const {
    std::vector<DialogHistoryItem> res;
    int start = (int)history_.size() - maxCount;
    if (start < 0) start = 0;
    for (size_t i = start; i < history_.size(); ++i) {
        res.push_back(history_[i]);
    }
    return res;
}

std::wstring AiMemory::GetLastTopic() const {
    if (history_.empty()) return L"";
    return history_.back().userQuery;
}

void AiMemory::ObserveStyle(const std::wstring& text) {
    std::wstring lower = text;
    for (auto& ch : lower) ch = (wchar_t)towlower(ch);
    auto adjust = [](int& score, bool positive) { score = (std::max)(0, (std::min)(20, score + (positive ? 1 : -1))); };
    bool casual = lower.find(L"бро") != std::wstring::npos || lower.find(L"брат") != std::wstring::npos ||
        lower.find(L"чё") != std::wstring::npos || lower.find(L"че ") != std::wstring::npos || lower.find(L"го ") != std::wstring::npos;
    if (casual) adjust(casualStyle_, true);
    else if (text.size() > 15) adjust(casualStyle_, false);
    size_t words = 0; bool inWord = false;
    for (wchar_t c : text) { if (iswalnum(c)) { if (!inWord) ++words; inWord = true; } else inWord = false; }
    adjust(conciseStyle_, words <= 8);
    Save();
}

bool AiMemory::PrefersConcise() const { return conciseStyle_ >= 3; }
bool AiMemory::PrefersCasual() const { return casualStyle_ >= 3; }

// ============================================================================
// AiBrain Implementation (General AI Brain & Automation)
// ============================================================================

AiBrain& AiBrain::Instance() {
    static AiBrain inst;
    return inst;
}

AiBrain::AiBrain() {}
AiBrain::~AiBrain() {}

void AiBrain::Init() {
    AiMemory::Instance().Load();
}

std::wstring AiBrain::GetCurrentTimeRu() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf_s(buf, L"Сейчас ровно %02d:%02d, братуха.", st.wHour, st.wMinute);
    return buf;
}

std::wstring AiBrain::GetCurrentDateRu() {
    SYSTEMTIME st;
    GetLocalTime(&st);

    static const wchar_t* months[] = {
        L"января", L"февраля", L"марта", L"апреля", L"мая", L"июня",
        L"июля", L"августа", L"сентября", L"октября", L"ноября", L"декабря"
    };
    static const wchar_t* days[] = {
        L"воскресенье", L"понедельник", L"вторник", L"среда", L"четверг", L"пятница", L"суббота"
    };

    int mIdx = (st.wMonth >= 1 && st.wMonth <= 12) ? st.wMonth - 1 : 10;
    int dIdx = (st.wDayOfWeek <= 6) ? st.wDayOfWeek : 0;

    wchar_t buf[128];
    swprintf_s(buf, L"Сегодня %d %s, %s. Отличный день для побед!", st.wDay, months[mIdx], days[dIdx]);
    return buf;
}

void AiBrain::SearchWeb(const std::wstring& query) {
    std::wstring encodedQuery = L"";
    for (wchar_t ch : query) {
        if (ch == L' ') encodedQuery += L"+";
        else encodedQuery += ch;
    }
    std::wstring url = L"https://ya.ru/search/?text=" + encodedQuery;
    ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void AiBrain::MinimizeAllWindows() {
    keybd_event(VK_LWIN, 0, 0, 0);
    keybd_event('D', 0, 0, 0);
    keybd_event('D', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
}

void AiBrain::TakeScreenshot() {
    keybd_event(VK_SNAPSHOT, 0, 0, 0);
    keybd_event(VK_SNAPSHOT, 0, KEYEVENTF_KEYUP, 0);
}

void AiBrain::ControlMedia(int action) {
    BYTE vk = 0;
    if (action == 0) vk = VK_MEDIA_PLAY_PAUSE;
    else if (action == 1) vk = VK_MEDIA_NEXT_TRACK;
    else if (action == 2) vk = VK_MEDIA_PREV_TRACK;
    else if (action == 3) {
        // Volume up
        for (int i = 0; i < 4; ++i) {
            keybd_event(VK_VOLUME_UP, 0, 0, 0);
            keybd_event(VK_VOLUME_UP, 0, KEYEVENTF_KEYUP, 0);
            Sleep(20);
        }
        return;
    }
    else if (action == 4) {
        // Volume down
        for (int i = 0; i < 4; ++i) {
            keybd_event(VK_VOLUME_DOWN, 0, 0, 0);
            keybd_event(VK_VOLUME_DOWN, 0, KEYEVENTF_KEYUP, 0);
            Sleep(20);
        }
        return;
    }
    else if (action == 5) vk = VK_VOLUME_MUTE;

    if (vk != 0) {
        keybd_event(vk, 0, 0, 0);
        Sleep(50);
        keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
    }
}

void AiBrain::LaunchApp(const std::wstring& appName) {
    if (appName == L"dota") {
        ShellExecuteW(NULL, L"open", L"steam://rungameid/570", NULL, NULL, SW_SHOWNORMAL);
    }
    else if (appName == L"steam") {
        ShellExecuteW(NULL, L"open", L"steam://open/main", NULL, NULL, SW_SHOWNORMAL);
    }
    else if (appName == L"youtube") {
        ShellExecuteW(NULL, L"open", L"https://youtube.com", NULL, NULL, SW_SHOWNORMAL);
    }
    else if (appName == L"browser") {
        ShellExecuteW(NULL, L"open", L"https://ya.ru", NULL, NULL, SW_SHOWNORMAL);
    }
    else if (appName == L"yandex") {
        wchar_t local[MAX_PATH] = {};
        GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
        std::wstring paths[] = {
            std::wstring(local) + L"\\Yandex\\YandexBrowser\\Application\\browser.exe",
            L"C:\\Program Files (x86)\\Yandex\\YandexBrowser\\Application\\browser.exe",
            L"C:\\Program Files\\Yandex\\YandexBrowser\\Application\\browser.exe"
        };
        for (const auto& path : paths) {
            if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
                ShellExecuteW(NULL, L"open", path.c_str(), L"https://ya.ru", NULL, SW_SHOWNORMAL);
                return;
            }
        }
        ShellExecuteW(NULL, L"open", L"https://ya.ru", NULL, NULL, SW_SHOWNORMAL);
    }
}

bool AiBrain::OpenMusicPlaylist(const std::wstring& serviceOverride) {
    if (!serviceOverride.empty()) {
        AiMemory::Instance().SetMusicService(serviceOverride);
    }

    std::wstring customUrl = AiMemory::Instance().GetCustomPlaylistUrl();
    if (!customUrl.empty()) {
        ShellExecuteW(NULL, L"open", customUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return true;
    }

    std::wstring s = AiMemory::Instance().GetMusicService();
    for (auto& ch : s) ch = (wchar_t)towlower(ch);

    if (s == L"spotify") {
        INT_PTR res = (INT_PTR)ShellExecuteW(NULL, L"open", L"spotify:", NULL, NULL, SW_SHOWNORMAL);
        if (res <= 32) {
            ShellExecuteW(NULL, L"open", L"https://open.spotify.com", NULL, NULL, SW_SHOWNORMAL);
        }
        return true;
    }
    else if (s == L"vk") {
        ShellExecuteW(NULL, L"open", L"https://vk.com/audio", NULL, NULL, SW_SHOWNORMAL);
        return true;
    }
    else if (s == L"youtube") {
        ShellExecuteW(NULL, L"open", L"https://music.youtube.com", NULL, NULL, SW_SHOWNORMAL);
        return true;
    }
    else {
        ShellExecuteW(NULL, L"open", L"https://music.yandex.ru", NULL, NULL, SW_SHOWNORMAL);
        return true;
    }
}

bool AiBrain::ProcessSpeech(const std::wstring& userSpeech, std::wstring& outReply) {
    std::wstring text = userSpeech;
    for (auto& ch : text) ch = (wchar_t)towlower(ch);
    AiMemory::Instance().ObserveStyle(userSpeech);

    // Live weather is fetched only after an explicit request, so idle listening stays offline.
    if (text.find(L"погод") != std::wstring::npos || text.find(L"температур") != std::wstring::npos) {
        size_t marker = text.find(L"в ");
        if (marker == std::wstring::npos) marker = text.find(L"в городе ");
        std::wstring city;
        if (marker != std::wstring::npos) {
            size_t start = marker + (text.compare(marker, 9, L"в городе ") == 0 ? 9 : 2);
            city = text.substr(start);
        } else if (text.find(L"смирных") != std::wstring::npos) city = L"Смирных";
        while (!city.empty() && (city.back() == L'.' || city.back() == L'?' || city.back() == L'!' || city.back() == L',')) city.pop_back();
        if (city.empty()) {
            outReply = L"В каком городе посмотреть погоду?";
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }

        std::string geoBody, weatherBody;
        std::wstring geoUrl = L"https://geocoding-api.open-meteo.com/v1/search?name=" + Utf8ToWide(UrlEncodeUtf8(city)) + L"&count=1&language=ru&format=json";
        try {
            if (HttpGetUtf8(geoUrl, geoBody)) {
                auto geo = nlohmann::json::parse(geoBody);
                if (geo.contains("results") && geo["results"].is_array() && !geo["results"].empty()) {
                    auto place = geo["results"][0];
                    std::string lat = std::to_string(place.value("latitude", 0.0));
                    std::string lon = std::to_string(place.value("longitude", 0.0));
                    std::wstring weatherUrl = L"https://api.open-meteo.com/v1/forecast?latitude=" + Utf8ToWide(lat) + L"&longitude=" + Utf8ToWide(lon) + L"&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m&timezone=auto";
                    if (HttpGetUtf8(weatherUrl, weatherBody)) {
                        auto weather = nlohmann::json::parse(weatherBody);
                        auto current = weather.at("current");
                        int degrees = (int)std::lround(current.value("temperature_2m", 0.0));
                        int feels = (int)std::lround(current.value("apparent_temperature", (double)degrees));
                        int humidity = current.value("relative_humidity_2m", 0);
                        int code = current.value("weather_code", 0);
                        int wind = (int)std::lround(current.value("wind_speed_10m", 0.0));
                        std::wstring displayCity = place.value("name", std::string()) .empty() ? city : Utf8ToWide(place.value("name", std::string()));
                        outReply = displayCity + L": сейчас " + std::to_wstring(degrees) + L" градусов, " + WeatherCodeRu(code) +
                            L", ощущается как " + std::to_wstring(feels) + L". Влажность " + std::to_wstring(humidity) +
                            L" процентов, ветер " + std::to_wstring(wind) + L" километров в час.";
                    }
                }
            }
        } catch (...) {}
        if (outReply.empty()) outReply = L"Не получилось получить погоду для «" + city + L"». Проверь подключение к интернету или название города.";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }

    std::wstring mathReply;
    if (TrySimpleArithmetic(text, mathReply)) {
        outReply = mathReply;
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }

    // 1. GENERAL MEMORY: REMEMBERING & RECALLING
    if (text.find(L"что ты помнишь") != std::wstring::npos || text.find(L"что ты знаешь обо мне") != std::wstring::npos ||
        text.find(L"твоя память") != std::wstring::npos || text.find(L"вспомни всё") != std::wstring::npos) {
        outReply = AiMemory::Instance().GetMemorySummary();
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"забудь всё") != std::wstring::npos || text.find(L"очисти память") != std::wstring::npos ||
        text.find(L"сбрось память") != std::wstring::npos) {
        AiMemory::Instance().ClearMemory();
        outReply = L"Память полностью очищена! Можешь задать всё с чистого листа.";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"запомни что") != std::wstring::npos || text.find(L"запомни") != std::wstring::npos ||
        text.find(L"сохрани в память") != std::wstring::npos) {
        size_t pos = text.find(L"запомни");
        if (pos == std::wstring::npos) pos = text.find(L"сохрани");
        std::wstring fact = text.substr(pos + 7);
        while (!fact.empty() && (fact[0] == L' ' || fact[0] == L',' || fact[0] == L':')) fact.erase(0, 1);
        if (fact.rfind(L"что ", 0) == 0) fact.erase(0, 4);

        if (!fact.empty()) {
            AiMemory::Instance().SetFact(fact, fact);
            outReply = L"Записал в память: \"" + fact + L"\". Можешь быть уверен, я ничего не забуду!";
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }
    }
    // Check if user is asking to recall a specific fact ("где...", "какой...", "вспомни...")
    if (text.find(L"вспомни") != std::wstring::npos || text.find(L"где мой") != std::wstring::npos ||
        text.find(L"какой мой") != std::wstring::npos) {
        std::wstring matched = AiMemory::Instance().FindMatchingFact(text);
        if (!matched.empty()) {
            outReply = matched;
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }
    }

    // 2. TIME & DATE UTILITIES
    if (text.find(L"сколько времени") != std::wstring::npos || text.find(L"который час") != std::wstring::npos ||
        text.find(L"точное время") != std::wstring::npos) {
        outReply = GetCurrentTimeRu();
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"какое сегодня число") != std::wstring::npos || text.find(L"какая дата") != std::wstring::npos ||
        text.find(L"какой день недели") != std::wstring::npos || text.find(L"какой сегодня день") != std::wstring::npos) {
        outReply = GetCurrentDateRu();
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }

    // 3. DESKTOP AUTOMATION (Search, Minimize, Screenshot)
    if (text.find(L"загугли") != std::wstring::npos || text.find(L"найди в интернете") != std::wstring::npos ||
        text.find(L"поищи в интернете") != std::wstring::npos) {
        size_t pos = text.find(L"загугли");
        if (pos == std::wstring::npos) pos = text.find(L"интернете");
        std::wstring q = text.substr(pos + (pos == text.find(L"загугли") ? 7 : 9));
        while (!q.empty() && (q[0] == L' ' || q[0] == L',' || q[0] == L':')) q.erase(0, 1);
        if (!q.empty()) {
            SearchWeb(q);
            outReply = L"Ищу в интернете: \"" + q + L"\". Открыл результаты в браузере!";
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }
    }
    if (text.find(L"сверни окна") != std::wstring::npos || text.find(L"сверни всё") != std::wstring::npos ||
        text.find(L"рабочий стол") != std::wstring::npos) {
        MinimizeAllWindows();
        outReply = L"Свернул все окна на рабочий стол!";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"скриншот") != std::wstring::npos || text.find(L"сделай скрин") != std::wstring::npos ||
        text.find(L"заскринь") != std::wstring::npos) {
        TakeScreenshot();
        outReply = L"Скриншот готов! Скопирован в буфер обмена.";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }

    // 4. APP LAUNCHERS
    if (text.find(L"открой доту") != std::wstring::npos || text.find(L"запусти доту") != std::wstring::npos) {
        LaunchApp(L"dota");
        outReply = L"Запускаю Доту! Вражеский трон уже дрожит!";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"открой стим") != std::wstring::npos || text.find(L"запусти стим") != std::wstring::npos) {
        LaunchApp(L"steam");
        outReply = L"Открываю Стим, братуха.";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"открой ютуб") != std::wstring::npos || text.find(L"запусти ютуб") != std::wstring::npos) {
        LaunchApp(L"youtube");
        outReply = L"Залетаем на Ютуб за свежими нарезками и роликами!";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"открой браузер") != std::wstring::npos || text.find(L"запусти браузер") != std::wstring::npos) {
        LaunchApp(L"browser");
        outReply = L"Открываю браузер.";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if ((text.find(L"открой яндекс") != std::wstring::npos || text.find(L"запусти яндекс") != std::wstring::npos) &&
        text.find(L"музык") == std::wstring::npos && text.find(L"плейлист") == std::wstring::npos) {
        LaunchApp(L"yandex");
        outReply = L"Открываю Яндекс Браузер.";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }

    // 5. MUSIC / PLAYLIST (ONLY when user explicitly requests music or playlist)
    bool explicitlyMentionsMusic = (text.find(L"плейлист") != std::wstring::npos ||
                                    text.find(L"музык") != std::wstring::npos ||
                                    text.find(L"песн") != std::wstring::npos ||
                                    text.find(L"трек") != std::wstring::npos);

    bool mentionsSpotify = (text.find(L"спотифай") != std::wstring::npos || text.find(L"spotify") != std::wstring::npos);
    bool mentionsYandex = (text.find(L"яндекс") != std::wstring::npos || text.find(L"yandex") != std::wstring::npos);
    bool mentionsVk = (text.find(L" вк") != std::wstring::npos || text.find(L"вконтакт") != std::wstring::npos);
    bool mentionsYoutube = (text.find(L"ютуб") != std::wstring::npos || text.find(L"youtube") != std::wstring::npos);

    if (explicitlyMentionsMusic) {
        if (mentionsSpotify) {
            AiMemory::Instance().SetMusicService(L"spotify");
            OpenMusicPlaylist(L"spotify");
            outReply = L"Запомнил! Открываю Спотифай.";
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }
        else if (mentionsYandex) {
            AiMemory::Instance().SetMusicService(L"yandex");
            OpenMusicPlaylist(L"yandex");
            outReply = L"Запомнил! Открываю Яндекс Музыку.";
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }
        else if (mentionsVk) {
            AiMemory::Instance().SetMusicService(L"vk");
            OpenMusicPlaylist(L"vk");
            outReply = L"Запомнил! Открываю ВК Музыку.";
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }
        else if (mentionsYoutube) {
            AiMemory::Instance().SetMusicService(L"youtube");
            OpenMusicPlaylist(L"youtube");
            outReply = L"Запомнил! Открываю Ютуб Музыку.";
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }
        else if (text.find(L"включи") != std::wstring::npos || text.find(L"вруби") != std::wstring::npos ||
                 text.find(L"поставь") != std::wstring::npos) {
            OpenMusicPlaylist();
            std::wstring serviceRu = AiMemory::Instance().GetMusicServiceNameRu();
            outReply = L"Включаю музыку в " + serviceRu + L"!";
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }
    }

    // Launch any installed desktop app that has a Start Menu shortcut.
    size_t openAt = text.find(L"открой ");
    size_t runAt = text.find(L"запусти ");
    size_t commandAt = openAt != std::wstring::npos ? openAt : runAt;
    if (commandAt != std::wstring::npos) {
        size_t commandLen = commandAt == openAt ? 7 : 8;
        std::wstring appName = text.substr(commandAt + commandLen);
        while (!appName.empty() && (appName.front() == L' ' || appName.front() == L',' || appName.front() == L':')) appName.erase(appName.begin());
        while (!appName.empty() && (appName.back() == L'.' || appName.back() == L'?' || appName.back() == L'!')) appName.pop_back();
        if (!appName.empty()) {
            if (LaunchStartMenuShortcut(appName)) outReply = L"Открываю " + appName + L".";
            else outReply = L"Не нашёл ярлык «" + appName + L"» в меню Пуск. Добавь ярлык приложения в меню Пуск и попробуй ещё раз.";
            AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
            return true;
        }
    }

    // Media keys
    if (text.find(L"пауза") != std::wstring::npos || text.find(L"стоп музык") != std::wstring::npos) {
        ControlMedia(0);
        outReply = L"Поставил на паузу, братан!";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"следующий трек") != std::wstring::npos || text.find(L"переключи песн") != std::wstring::npos ||
        text.find(L"скипни трек") != std::wstring::npos) {
        ControlMedia(1);
        outReply = L"Переключил трек!";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"громче") != std::wstring::npos || text.find(L"прибавь звук") != std::wstring::npos) {
        ControlMedia(3);
        outReply = L"Сделал погромче!";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"тише") != std::wstring::npos || text.find(L"убавь звук") != std::wstring::npos) {
        ControlMedia(4);
        outReply = L"Сделал потише.";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }
    if (text.find(L"выключи звук") != std::wstring::npos || text.find(L"без звука") != std::wstring::npos) {
        ControlMedia(5);
        outReply = L"Звук выключен.";
        AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
        return true;
    }

    // 6. NATURAL CONVERSATIONAL AI RESPONSES (Clean, organic, no repetitive keywords!)
    outReply = GenerateConversationalReply(userSpeech);
    if (AiMemory::Instance().PrefersConcise()) {
        size_t end = outReply.find_first_of(L".!?", 35);
        if (end != std::wstring::npos) outReply.resize(end + 1);
    }
    if (AiMemory::Instance().PrefersCasual() && outReply.rfind(L"Бро, ", 0) != 0) {
        outReply = L"Бро, " + outReply;
    }
    AiMemory::Instance().AddDialogHistory(userSpeech, outReply);
    return true;
}

std::wstring AiBrain::GenerateConversationalReply(const std::wstring& text) {
    std::wstring systemPrompt = L"Ты Свин — голосовой помощник на компьютере пользователя. Понимай естественную русскую речь, отвечай по существу, без заготовленных приветствий, кратко и без Markdown, чтобы ответ можно было произнести вслух. Решай простые задачи и объясняй ход ответа только если попросили. Для Доты учитывай переданный текущий драфт и предлагай героя только из переданного списка доступных кандидатов; назови конкретный пик и коротко объясни почему. Не выдумывай, какие герои видны, если список не передан.";
    if (AiMemory::Instance().PrefersCasual()) systemPrompt += L" Общайся неформально и на «ты», подстраивайся под разговорную манеру пользователя.";
    if (AiMemory::Instance().PrefersConcise()) systemPrompt += L" Пользователь предпочитает очень короткие ответы.";

    std::wstring context;
    auto history = AiMemory::Instance().GetRecentHistory(5);
    for (const auto& item : history) {
        context += L"Пользователь: " + item.userQuery + L"\nСвин: " + item.pigResponse + L"\n";
    }
    std::string currentHero = HeroAdvisor::Instance().GetCurrentHeroName();
    if (!currentHero.empty()) {
        context += L"Текущий герой пользователя в Доте: " + Utf8ToWide(currentHero) + L".\n";
    }
    context += HeroAdvisor::Instance().GetDraftContext();
    std::wstring userPrompt = context + L"Последняя реплика пользователя: " + text;
    if (!LocalAiEngine::Instance().IsModelInstalled()) {
        VoiceAssistant::Instance().NotifyStatus(L"Первый запуск: загружаю бесплатную локальную модель ИИ размером около 563 мегабайт. Это займёт несколько минут; нужен интернет только для этой загрузки.");
    }
    std::wstring answer;
    if (LocalAiEngine::Instance().Generate(systemPrompt, userPrompt, answer)) return answer;
    return L"Не получилось запустить локальный ИИ. Проверь подключение к интернету для первой загрузки модели и повтори вопрос.";
}
