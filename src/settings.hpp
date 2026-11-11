#pragma once
#include <windows.h>
#include <string>

namespace proxychecker {

struct Config {
    std::wstring proxyHost = L"172.16.136.19";
    DWORD proxyPort = 3128;
    DWORD probeIntervalSeconds = 10;
    DWORD probeTimeoutSeconds = 12;
    DWORD downConfirmSeconds = 90;
    DWORD recoverySuccesses = 2;
    bool healthchecksEnabled = false;
    std::wstring healthchecksUrl;
    DWORD heartbeatIntervalSeconds = 60;
    DWORD heartbeatTimeoutSeconds = 10;
};

std::wstring RegistryPath();
bool LoadConfig(Config& config, std::wstring& error);
bool SaveConfig(const Config& config, std::wstring& error);
bool ValidateConfig(const Config& config, std::wstring& error);
bool ValidHealthchecksUrl(const std::wstring& url);

}  // namespace proxychecker
