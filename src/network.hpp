#pragma once
#include "settings.hpp"
#include <windows.h>
#include <string>

namespace proxychecker {

struct HttpResult {
    DWORD status = 0;
    DWORD error = 0;
    long long milliseconds = 0;
    bool reachable = false;
    bool heartbeatAccepted = false;
    std::wstring details;
};

HttpResult HttpGet(const std::wstring& url, const Config& config,
    DWORD timeoutSeconds, bool healthchecks = false);
std::wstring SystemError(DWORD error);

}  // namespace proxychecker
