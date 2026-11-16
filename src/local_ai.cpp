#include "local_ai.h"
#include "config.h"
#include <nlohmann/json.hpp>
#include <wininet.h>
#include <urlmon.h>
#include <bcrypt.h>
#include <algorithm>
#include <vector>
#include <iterator>
#include <shlobj.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "bcrypt.lib")

using nlohmann::json;

static std::string WideToUtf8Local(const std::wstring& value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string out(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &out[0], size, nullptr, nullptr);
    out.pop_back();
    return out;
}

static std::wstring Utf8ToWideLocal(const std::string& value) {
    if (value.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 1) return {};
    std::wstring out(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &out[0], size);
    out.pop_back();
    return out;
}

static bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static bool VerifyModelSha256(const std::wstring& path) {
    static const char expected[] = "57d1997790d1744fba5b40a7317df71ea5e2acee28c47e78f0cce39c0703f8cf";
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0, returned = 0, hashSize = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    bool ok = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objectSize, sizeof(objectSize), &returned, 0) >= 0 &&
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, (PUCHAR)&hashSize, sizeof(hashSize), &returned, 0) >= 0;
    std::vector<UCHAR> object(objectSize), digest(hashSize), buffer(1024 * 1024);
    if (ok) ok = BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) >= 0;
    HANDLE file = ok ? CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr) : INVALID_HANDLE_VALUE;
    if (file == INVALID_HANDLE_VALUE) ok = false;
    while (ok) {
        DWORD read = 0;
        if (!ReadFile(file, buffer.data(), (DWORD)buffer.size(), &read, nullptr)) { ok = false; break; }
        if (!read) break;
        if (BCryptHashData(hash, buffer.data(), read, 0) < 0) { ok = false; break; }
    }
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (ok) ok = BCryptFinishHash(hash, digest.data(), hashSize, 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok || hashSize != 32) return false;
    static const char* hex = "0123456789abcdef";
    std::string actual;
    for (UCHAR c : digest) { actual += hex[c >> 4]; actual += hex[c & 0x0f]; }
    return actual == expected;
}

static bool HttpReadLocal(unsigned short port, const wchar_t* method, const wchar_t* path,
    const std::string* postBody, std::string& response, DWORD timeoutMs) {
    HINTERNET session = InternetOpenW(L"SvinLocalAI/6.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!session) return false;
    InternetSetOptionW(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(session, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionW(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    HINTERNET connection = InternetConnectW(session, L"127.0.0.1", port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    HINTERNET request = connection ? HttpOpenRequestW(connection, method, path, nullptr, nullptr, nullptr, INTERNET_FLAG_NO_CACHE_WRITE, 0) : nullptr;
    bool ok = false;
    if (request) {
        if (postBody) {
            const wchar_t* headers = L"Content-Type: application/json; charset=utf-8\r\n";
            ok = HttpSendRequestW(request, headers, (DWORD)-1L, (LPVOID)postBody->data(), (DWORD)postBody->size()) != FALSE;
        } else {
            ok = HttpSendRequestW(request, nullptr, 0, nullptr, 0) != FALSE;
        }
        DWORD status = 0, statusSize = sizeof(status);
        if (ok) ok = HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &statusSize, nullptr) && status >= 200 && status < 300;
        if (ok) {
            char chunk[8192]; DWORD received = 0;
            while (InternetReadFile(request, chunk, sizeof(chunk), &received) && received) {
                response.append(chunk, received);
                if (response.size() > 2 * 1024 * 1024) { ok = false; break; }
            }
        }
        InternetCloseHandle(request);
    }
    if (connection) InternetCloseHandle(connection);
    InternetCloseHandle(session);
    return ok;
}

LocalAiEngine& LocalAiEngine::Instance() {
    static LocalAiEngine engine;
    if (!engine.lockInitialized_) {
        InitializeCriticalSection(&engine.lock_);
        engine.lockInitialized_ = true;
    }
    return engine;
}

LocalAiEngine::~LocalAiEngine() { Shutdown(); }

bool LocalAiEngine::EnsureModel(std::wstring& modelPath) {
    wchar_t envPath[MAX_PATH * 4] = {};
    DWORD envLen = GetEnvironmentVariableW(L"SVIN_AI_MODEL_PATH", envPath, (DWORD)std::size(envPath));
    if (envLen > 0 && envLen < std::size(envPath) && FileExists(envPath) && VerifyModelSha256(envPath)) {
        modelPath = envPath;
        return true;
    }

    std::wstring aiDir = ConfigManager::GetExeDir() + L"\\ai";
    SHCreateDirectoryExW(nullptr, aiDir.c_str(), nullptr);
    modelPath = aiDir + L"\\Qwen3.5-0.8B-Q4_0.gguf";
    if (FileExists(modelPath)) {
        if (VerifyModelSha256(modelPath)) return true;
        DeleteFileW(modelPath.c_str());
    }

    std::wstring partialPath = modelPath + L".download";
    DeleteFileW(partialPath.c_str());
    const wchar_t* modelUrl = L"https://github.com/meantclient/WidgetBadPiggies/releases/download/v6.0/Qwen3.5-0.8B-Q4_0.gguf";
    HRESULT hr = URLDownloadToFileW(nullptr, modelUrl, partialPath.c_str(), 0, nullptr);
    if (FAILED(hr) || !VerifyModelSha256(partialPath)) {
        DeleteFileW(partialPath.c_str());
        return false;
    }
    if (!MoveFileExW(partialPath.c_str(), modelPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(partialPath.c_str());
        return false;
    }
    return true;
}

bool LocalAiEngine::IsServerReady() const {
    if (!process_ || WaitForSingleObject(process_, 0) != WAIT_TIMEOUT || !port_) return false;
    std::string response;
    return HttpReadLocal(port_, L"GET", L"/health", nullptr, response, 1000);
}

bool LocalAiEngine::EnsureServer(const std::wstring& modelPath) {
    if (IsServerReady()) return true;
    if (process_) {
        if (!processInJob_) TerminateProcess(process_, 0);
        CloseHandle(process_); process_ = nullptr;
    }
    if (processThread_) { CloseHandle(processThread_); processThread_ = nullptr; }
    if (job_) { CloseHandle(job_); job_ = nullptr; }
    processInJob_ = false;

    std::wstring exe = ConfigManager::GetExeDir() + L"\\llama\\llama-server.exe";
    if (!FileExists(exe)) return false;
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    // Keep the assistant responsive without taking over a small CPU.
    unsigned int processorCount = static_cast<unsigned int>(si.dwNumberOfProcessors);
    unsigned int threads = (std::max)(1u, (std::min)(2u, (std::max)(1u, processorCount / 2u)));
    port_ = (unsigned short)(41000 + (GetTickCount() % 18000));
    std::wstring command = L"\"" + exe + L"\" --model \"" + modelPath + L"\" --host 127.0.0.1 --port " +
        std::to_wstring(port_) + L" --ctx-size 2048 --parallel 1 --threads " + std::to_wstring(threads) +
        L" --threads-batch " + std::to_wstring(threads) +
        L" --n-predict 140 --sleep-idle-seconds 30 --prio -1 --no-webui --log-disable";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    std::vector<wchar_t> writable(command.begin(), command.end());
    writable.push_back(L'\0');
    if (!CreateProcessW(exe.c_str(), writable.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
        ConfigManager::GetExeDir().c_str(), &startup, &info)) return false;
    process_ = info.hProcess;
    processThread_ = info.hThread;
    SetPriorityClass(process_, BELOW_NORMAL_PRIORITY_CLASS);
    job_ = CreateJobObjectW(nullptr, nullptr);
    if (job_) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        processInJob_ = AssignProcessToJobObject(job_, process_) != FALSE;
        if (!processInJob_) { CloseHandle(job_); job_ = nullptr; }
    }
    for (int i = 0; i < 180; ++i) {
        if (WaitForSingleObject(process_, 0) != WAIT_TIMEOUT) return false;
        if (IsServerReady()) return true;
        Sleep(500);
    }
    return false;
}

bool LocalAiEngine::SendChatRequest(const std::wstring& systemPrompt, const std::wstring& userPrompt, std::wstring& reply) {
    json request;
    request["model"] = "svin-local";
    request["messages"] = json::array({
        {{"role", "system"}, {"content", WideToUtf8Local(systemPrompt)}},
        {{"role", "user"}, {"content", WideToUtf8Local(userPrompt)}}
    });
    request["temperature"] = 0.25;
    request["max_tokens"] = 180;
    request["stream"] = false;
    request["chat_template_kwargs"] = {{"enable_thinking", false}};
    std::string response;
    std::wstring path = L"/v1/chat/completions";
    std::string postBody = request.dump();
    if (!HttpReadLocal(port_, L"POST", path.c_str(), &postBody, response, 90000)) return false;
    try {
        auto parsed = json::parse(response);
        std::string content = parsed.at("choices").at(0).at("message").at("content").get<std::string>();
        reply = Utf8ToWideLocal(content);
        while (!reply.empty() && iswspace(reply.front())) reply.erase(reply.begin());
        while (!reply.empty() && iswspace(reply.back())) reply.pop_back();
        return !reply.empty();
    } catch (...) { return false; }
}

bool LocalAiEngine::Generate(const std::wstring& systemPrompt, const std::wstring& userPrompt, std::wstring& reply) {
    EnterCriticalSection(&lock_);
    std::wstring modelPath;
    bool ok = EnsureModel(modelPath) && EnsureServer(modelPath) && SendChatRequest(systemPrompt, userPrompt, reply);
    LeaveCriticalSection(&lock_);
    return ok;
}

bool LocalAiEngine::IsModelInstalled() const {
    wchar_t envPath[MAX_PATH * 4] = {};
    DWORD envLen = GetEnvironmentVariableW(L"SVIN_AI_MODEL_PATH", envPath, (DWORD)std::size(envPath));
    if (envLen > 0 && envLen < std::size(envPath) && FileExists(envPath)) return true;
    std::wstring modelPath = ConfigManager::GetExeDir() + L"\\ai\\Qwen3.5-0.8B-Q4_0.gguf";
    return FileExists(modelPath);
}

void LocalAiEngine::Shutdown() {
    if (lockInitialized_) EnterCriticalSection(&lock_);
    if (job_) { CloseHandle(job_); job_ = nullptr; }
    if (process_) { if (!processInJob_) TerminateProcess(process_, 0); CloseHandle(process_); process_ = nullptr; }
    processInJob_ = false;
    if (processThread_) { CloseHandle(processThread_); processThread_ = nullptr; }
    if (lockInitialized_) LeaveCriticalSection(&lock_);
}
