#include "engine.hpp"
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace proxychecker {
namespace {
constexpr struct Endpoint { const wchar_t* name; const wchar_t* url; } kEndpoints[] = {
    {L"Cloudflare", L"https://www.cloudflare.com/cdn-cgi/trace"},
    {L"Microsoft", L"https://www.microsoft.com/"},
    {L"Telegram", L"https://api.telegram.org/"},
    {L"Google", L"https://www.google.com/generate_204"}
};

std::string Utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const auto count = WideCharToMultiByte(CP_UTF8, 0, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string out(static_cast<size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
        out.data(), count, nullptr, nullptr);
    return out;
}

void AppendLog(const std::wstring& name, const std::wstring& text) {
    try {
        const std::filesystem::path dir(LogFolder());
        std::filesystem::create_directories(dir);
        const auto path = dir / name;
        std::ofstream stream(path, std::ios::binary | std::ios::app);
        if (stream) stream << Utf8(L"[" + CurrentTime() + L"] " + text + L"\r\n");
    } catch (...) {
        // Logging must never stop monitoring.
    }
}

std::wstring DateStamp() {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t b[16]{};
    swprintf_s(b, L"%04u%02u%02u", t.wYear, t.wMonth, t.wDay);
    return b;
}

void CleanupLogs() {
    try {
        const std::filesystem::path dir(LogFolder());
        if (!std::filesystem::exists(dir)) return;
        const auto cutoff = std::filesystem::file_time_type::clock::now() -
            std::chrono::hours(24 * 14);
        for (const auto& item : std::filesystem::directory_iterator(dir)) {
            if (item.is_regular_file() && item.path().extension() == L".log" &&
                item.last_write_time() < cutoff) std::filesystem::remove(item.path());
        }
    } catch (...) {}
}

std::wstring Describe(const wchar_t* name, const HttpResult& r) {
    std::wostringstream out;
    out << name << L"  " << (r.reachable ? L"OK" : L"FAIL") <<
        L"  HTTP=" << r.status << L"  WinHTTP=" << r.error <<
        L"  Time=" << r.milliseconds << L"ms";
    if (!r.details.empty()) out << L"  " << r.details;
    return out.str();
}
}  // namespace

std::wstring CurrentTime() {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t b[32]{};
    swprintf_s(b, L"%04u-%02u-%02u %02u:%02u:%02u",
        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}

std::wstring LogFolder() {
    PWSTR base = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base))) {
        const std::wstring folder = std::wstring(base) + L"\\ProxyChecker\\Logs";
        CoTaskMemFree(base);
        return folder;
    }
    return L".\\ProxyChecker-Logs";
}

void LogEvent(const std::wstring& message) {
    AppendLog(L"events-" + DateStamp() + L".log", message);
}

void LogProbe(const std::wstring& message) {
    AppendLog(L"probes-" + DateStamp() + L".log", message);
}

Engine::Engine(HWND notify, Config config) : notify_(notify), config_(std::move(config)) {
    snapshot_.healthchecksEnabled = config_.healthchecksEnabled;
}

Engine::~Engine() { Stop(); }

void Engine::Start() {
    CleanupLogs();
    std::lock_guard lock(mutex_);
    if (!thread_.joinable()) {
        stop_ = false;
        thread_ = std::thread(&Engine::Worker, this);
    }
}

void Engine::Stop() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void Engine::Notify() const {
    if (notify_) PostMessageW(notify_, WM_PROXYCHECKER_UPDATE, 0, 0);
}

void Engine::UpdateConfig(const Config& config) {
    {
        std::lock_guard lock(mutex_);
        config_ = config;
        ++generation_;
        snapshot_.state = Status::Unknown;
        snapshot_.lastTransition = CurrentTime() + L" UNKNOWN (settings changed)";
        snapshot_.healthchecksEnabled = config.healthchecksEnabled;
        diagnosticsRequested_ = true;
    }
    wake_.notify_one();
    Notify();
}

void Engine::RequestDiagnostics() {
    {
        std::lock_guard lock(mutex_);
        diagnosticsRequested_ = true;
    }
    wake_.notify_one();
}

Snapshot Engine::GetSnapshot() const {
    std::lock_guard lock(mutex_);
    return snapshot_;
}

void Engine::Worker() {
    using Clock = StateTracker::Clock;
    StateTracker tracker;
    unsigned long long knownGeneration = static_cast<unsigned long long>(-1);
    Clock::time_point lastHeartbeat = Clock::time_point::min();
    while (true) {
        Config local;
        bool diagnostics = false;
        unsigned long long generation = 0;
        {
            std::lock_guard lock(mutex_);
            if (stop_) break;
            local = config_;
            diagnostics = diagnosticsRequested_;
            diagnosticsRequested_ = false;
            generation = generation_;
        }
        if (knownGeneration != generation) {
            tracker = StateTracker(static_cast<int>(local.downConfirmSeconds),
                static_cast<int>(local.recoverySuccesses));
            lastHeartbeat = Clock::time_point::min();
            knownGeneration = generation;
        }

        bool anySuccess = false;
        std::wostringstream diagnostic;
        diagnostic << L"Last diagnostic: " << CurrentTime() << L"\r\n";
        std::wstring lastProbe;
        for (const auto& ep : kEndpoints) {
            {
                std::lock_guard lock(mutex_);
                if (stop_ || generation != generation_) break;
            }
            const auto r = HttpGet(ep.url, local, local.probeTimeoutSeconds);
            lastProbe = CurrentTime() + L" " + Describe(ep.name, r);
            LogProbe(Describe(ep.name, r));
            diagnostic << Describe(ep.name, r) << L"\r\n";
            if (r.reachable) anySuccess = true;
            if (anySuccess && !diagnostics) break;
        }
        {
            std::lock_guard lock(mutex_);
            if (stop_) break;
            if (generation != generation_) continue;  // Discard stale proxy results.
        }
        const auto now = Clock::now();
        const auto transition = tracker.Probe(anySuccess, now);
        {
            std::lock_guard lock(mutex_);
            snapshot_.state = tracker.state();
            ++snapshot_.checks;
            snapshot_.lastProbe = lastProbe;
            if (diagnostics) {
                diagnostic << L"Route reachable: " << (anySuccess ? L"YES" : L"NO")
                    << L"\r\n";
                snapshot_.diagnostics = diagnostic.str();
            }
            if (transition.changed()) {
                snapshot_.lastTransition = CurrentTime() + L" " +
                    StatusName(transition.before) + L" -> " + StatusName(transition.after);
            }
        }
        if (transition.changed()) {
            LogEvent(L"State " + std::wstring(StatusName(transition.before)) +
                L" -> " + StatusName(transition.after));
        }
        Notify();

        // A heartbeat may only follow verified, current, healthy proxy traffic.
        if (ShouldSendHeartbeat(tracker.state(), local.healthchecksEnabled,
            !local.healthchecksUrl.empty(), now, lastHeartbeat,
            static_cast<int>(local.heartbeatIntervalSeconds))) {
            lastHeartbeat = now;
            const auto result = HttpGet(local.healthchecksUrl, local,
                local.heartbeatTimeoutSeconds, true);
            const auto description = CurrentTime() + std::wstring(
                result.heartbeatAccepted ? L" accepted" : L" failed") +
                L" HTTP=" + std::to_wstring(result.status) +
                L" WinHTTP=" + std::to_wstring(result.error) +
                L" Time=" + std::to_wstring(result.milliseconds) + L"ms" +
                L" " + result.details;
            LogEvent(L"Healthchecks heartbeat " + description);
            {
                std::lock_guard lock(mutex_);
                if (generation == generation_) snapshot_.lastHeartbeat = description;
            }
            Notify();
        }

        std::unique_lock lock(mutex_);
        wake_.wait_for(lock, std::chrono::seconds(local.probeIntervalSeconds),
            [&] { return stop_ || generation_ != generation || diagnosticsRequested_; });
        if (stop_) break;
    }
}

}  // namespace proxychecker
