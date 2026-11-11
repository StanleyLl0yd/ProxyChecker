#include "network.hpp"
#include <winhttp.h>
#include <algorithm>
#include <chrono>
#include <string>

namespace proxychecker {
namespace {
struct Handle {
    HINTERNET value = nullptr;
    explicit Handle(HINTERNET handle = nullptr) : value(handle) {}
    ~Handle() { if (value) WinHttpCloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    explicit operator bool() const noexcept { return value != nullptr; }
};

std::wstring ProxyAddress(const Config& config) {
    const auto& host = config.proxyHost;
    const bool ipv6 = host.find(L':') != std::wstring::npos && host.front() != L'[';
    return (ipv6 ? L"[" + host + L"]" : host) + L":" + std::to_wstring(config.proxyPort);
}
}  // namespace

std::wstring SystemError(DWORD error) {
    if (!error) return L"none";
    wchar_t* buffer = nullptr;
    const auto length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0,
        reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring message;
    if (length && buffer) message.assign(buffer, length);
    if (buffer) LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' ||
        message.back() == L'\n' || message.back() == L' ')) message.pop_back();
    if (message.empty()) message = L"WinHTTP/Windows error " + std::to_wstring(error);
    return message;
}

HttpResult HttpGet(const std::wstring& url, const Config& config,
    DWORD timeoutSeconds, bool healthchecks) {
    const auto started = std::chrono::steady_clock::now();
    HttpResult result;
    auto fail = [&](DWORD error, const wchar_t* stage) {
        result.error = error;
        result.details = std::wstring(stage) + L": " + SystemError(error);
    };
    do {
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        parts.dwSchemeLength = 1;
        parts.dwHostNameLength = 1;
        parts.dwUrlPathLength = 1;
        parts.dwExtraInfoLength = 1;
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) ||
            parts.nScheme != INTERNET_SCHEME_HTTPS ||
            !parts.lpszHostName || !parts.dwHostNameLength) {
            fail(GetLastError() ? GetLastError() : ERROR_INVALID_PARAMETER, L"Invalid HTTPS URL");
            break;
        }
        const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
        std::wstring path = parts.dwUrlPathLength ?
            std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) : L"/";
        if (parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

        const auto proxy = ProxyAddress(config);
        Handle session(WinHttpOpen(L"ProxyChecker/0.1",
            WINHTTP_ACCESS_TYPE_NAMED_PROXY, proxy.c_str(), WINHTTP_NO_PROXY_BYPASS, 0));
        if (!session) { fail(GetLastError(), L"WinHttpOpen"); break; }
        Handle connection(WinHttpConnect(session.value, host.c_str(), parts.nPort, 0));
        if (!connection) { fail(GetLastError(), L"WinHttpConnect"); break; }
        Handle request(WinHttpOpenRequest(connection.value, L"GET", path.c_str(),
            nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
        if (!request) { fail(GetLastError(), L"WinHttpOpenRequest"); break; }

        const auto timeout = static_cast<int>(std::clamp<DWORD>(timeoutSeconds, 2, 30) * 1000);
        if (!WinHttpSetTimeouts(request.value, timeout, timeout, timeout, timeout)) {
            fail(GetLastError(), L"WinHttpSetTimeouts");
            break;
        }
        // Never follow a redirect: a Healthchecks secret URL must not leave its intended host.
        DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!WinHttpSetOption(request.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect))) {
            fail(GetLastError(), L"WinHttpSetOption");
            break;
        }
        if (!WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            fail(GetLastError(), L"WinHttpSendRequest"); break;
        }
        if (!WinHttpReceiveResponse(request.value, nullptr)) {
            fail(GetLastError(), L"WinHttpReceiveResponse"); break;
        }
        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(request.value,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
            WINHTTP_NO_HEADER_INDEX)) {
            fail(GetLastError(), L"WinHttpQueryHeaders"); break;
        }
        result.status = status;
        result.reachable = status >= 200 && status < 400;
        if (!healthchecks) {
            result.details = L"HTTPS response";
            break;
        }
        // hc-ping.com may reply HTTP 200 with text "not found" or "rate limited":
        // a bare 200 is not proof of a recorded heartbeat.
        if (status != 200) {
            result.details = L"Healthchecks returned HTTP " + std::to_wstring(status);
            break;
        }
        char bytes[128]{};
        DWORD read = 0;
        if (!WinHttpReadData(request.value, bytes, sizeof(bytes), &read)) {
            fail(GetLastError(), L"WinHttpReadData"); break;
        }
        std::string body(bytes, bytes + read);
        while (!body.empty() && (body.back() == '\r' || body.back() == '\n' ||
            body.back() == ' ' || body.back() == '\t')) body.pop_back();
        result.heartbeatAccepted = body == "OK";
        result.details = result.heartbeatAccepted ?
            L"Healthchecks accepted heartbeat" : L"Healthchecks did not confirm heartbeat";
    } while (false);

    result.milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    return result;
}

}  // namespace proxychecker
