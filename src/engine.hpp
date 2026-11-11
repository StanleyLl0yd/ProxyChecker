#pragma once
#include "core.hpp"
#include "settings.hpp"
#include "network.hpp"
#include <windows.h>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <cstdint>

namespace proxychecker {

constexpr UINT WM_PROXYCHECKER_UPDATE = WM_APP + 28;

struct Snapshot {
    Status state = Status::Unknown;
    unsigned long long checks = 0;
    std::wstring lastProbe = L"Not checked";
    std::wstring lastTransition = L"Not observed";
    std::wstring lastHeartbeat = L"Not sent";
    std::wstring diagnostics = L"Click Diagnose to test all four endpoints.";
    bool healthchecksEnabled = false;
};

class Engine {
public:
    Engine(HWND notify, Config config);
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void Start();
    void Stop();
    void UpdateConfig(const Config& config);
    void RequestDiagnostics();
    Snapshot GetSnapshot() const;

private:
    void Worker();
    void Notify() const;
    HWND notify_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    Config config_;
    Snapshot snapshot_;
    bool stop_ = false;
    bool diagnosticsRequested_ = false;
    unsigned long long generation_ = 0;
    std::thread thread_;
};

std::wstring CurrentTime();
void LogEvent(const std::wstring& message);
void LogProbe(const std::wstring& message);
std::wstring LogFolder();

}  // namespace proxychecker
