#include "settings.hpp"
#include <wincrypt.h>
#include <algorithm>
#include <vector>
#include <cwctype>
#include <limits>

namespace proxychecker {
namespace {
constexpr wchar_t kKey[] = L"Software\\ProxyChecker";
constexpr wchar_t kSecretValue[] = L"HealthchecksUrlProtected";

class Key {
public:
    HKEY handle = nullptr;
    ~Key() { if (handle) RegCloseKey(handle); }
    Key(const Key&) = delete;
    Key& operator=(const Key&) = delete;
    Key() = default;
};

DWORD ReadDword(HKEY key, const wchar_t* name, DWORD fallback) {
    DWORD type = 0, size = sizeof(DWORD), value = fallback;
    if (RegQueryValueExW(key, name, nullptr, &type,
        reinterpret_cast<BYTE*>(&value), &size) != ERROR_SUCCESS ||
        type != REG_DWORD || size != sizeof(DWORD)) return fallback;
    return value;
}

std::wstring ReadString(HKEY key, const wchar_t* name, const std::wstring& fallback) {
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        type != REG_SZ || size < sizeof(wchar_t) || size > 4096 ||
        size % sizeof(wchar_t) != 0) return fallback;
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type,
        reinterpret_cast<BYTE*>(value.data()), &size) != ERROR_SUCCESS) return fallback;
    value.resize(wcsnlen_s(value.c_str(), value.size()));
    return value;
}

bool WriteDword(HKEY key, const wchar_t* name, DWORD value) {
    return RegSetValueExW(key, name, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value)) == ERROR_SUCCESS;
}

bool WriteString(HKEY key, const wchar_t* name, const std::wstring& value) {
    return RegSetValueExW(key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

bool ReadProtected(HKEY key, std::wstring& secret, std::wstring& error) {
    DWORD type = 0, size = 0;
    const auto result = RegQueryValueExW(key, kSecretValue, nullptr, &type, nullptr, &size);
    if (result == ERROR_FILE_NOT_FOUND) return true;
    if (result != ERROR_SUCCESS || type != REG_BINARY || size == 0 || size > 8192) {
        error = L"Stored Healthchecks credential has invalid registry format.";
        return false;
    }
    std::vector<BYTE> raw(size);
    if (RegQueryValueExW(key, kSecretValue, nullptr, &type, raw.data(), &size) != ERROR_SUCCESS) {
        error = L"Unable to read protected Healthchecks credential.";
        return false;
    }
    DATA_BLOB input{size, raw.data()};
    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
        CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        error = L"Windows DPAPI could not decrypt Healthchecks URL for this user.";
        return false;
    }
    const bool correct = output.cbData > 0 && output.cbData % sizeof(wchar_t) == 0 &&
        output.cbData < 4096;
    if (correct) secret.assign(reinterpret_cast<const wchar_t*>(output.pbData),
        output.cbData / sizeof(wchar_t));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);
    if (!correct) {
        error = L"Decrypted Healthchecks credential has invalid length.";
        return false;
    }
    return true;
}

bool WriteProtected(HKEY key, const std::wstring& secret) {
    if (secret.empty()) {
        const auto code = RegDeleteValueW(key, kSecretValue);
        return code == ERROR_SUCCESS || code == ERROR_FILE_NOT_FOUND;
    }
    DATA_BLOB plain{
        static_cast<DWORD>(secret.size() * sizeof(wchar_t)),
        reinterpret_cast<BYTE*>(const_cast<wchar_t*>(secret.data()))
    };
    DATA_BLOB encoded{};
    if (!CryptProtectData(&plain, L"ProxyChecker Healthchecks URL", nullptr,
        nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &encoded)) return false;
    const bool ok = RegSetValueExW(key, kSecretValue, 0, REG_BINARY,
        encoded.pbData, encoded.cbData) == ERROR_SUCCESS;
    SecureZeroMemory(encoded.pbData, encoded.cbData);
    LocalFree(encoded.pbData);
    return ok;
}

bool SafeHostChar(wchar_t c) {
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') ||
        (c >= L'0' && c <= L'9') || c == L'.' || c == L'-' ||
        c == L':' || c == L'[' || c == L']';
}
}  // namespace

std::wstring RegistryPath() { return L"HKCU\\Software\\ProxyChecker"; }

bool ValidHealthchecksUrl(const std::wstring& url) {
    constexpr wchar_t prefix[] = L"https://hc-ping.com/";
    const std::wstring pre = prefix;
    if (url.compare(0, pre.size(), pre) != 0) return false;
    const auto path = url.substr(pre.size());
    if (path.empty() || path.size() > 180 || path.back() == L'/' ||
        path.find(L"..") != std::wstring::npos) return false;
    const auto slash = path.find(L'/');
    if (slash != std::wstring::npos && path.find(L'/', slash + 1) != std::wstring::npos)
        return false;
    for (const auto c : path) {
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'-' || c == L'_' || c == L'/'))
            return false;
    }
    if (slash == 0 || slash == path.size() - 1) return false;
    // Reject reserved action suffixes; an accidental /fail must never be sent.
    if (slash != std::wstring::npos) {
        const auto suffix = path.substr(slash + 1);
        if (suffix == L"start" || suffix == L"fail" || suffix == L"log") return false;
    }
    return true;
}

bool ValidateConfig(const Config& c, std::wstring& error) {
    if (c.proxyHost.empty() || c.proxyHost.size() > 253 ||
        !std::all_of(c.proxyHost.begin(), c.proxyHost.end(), SafeHostChar)) {
        error = L"Proxy host must be an IP address or DNS name without URL scheme.";
        return false;
    }
    if (c.proxyPort == 0 || c.proxyPort > 65535) {
        error = L"Proxy port must be between 1 and 65535.";
        return false;
    }
    if (c.probeIntervalSeconds < 5 || c.probeIntervalSeconds > 300 ||
        c.probeTimeoutSeconds < 2 || c.probeTimeoutSeconds > 30 ||
        c.downConfirmSeconds < 30 || c.downConfirmSeconds > 3600 ||
        c.recoverySuccesses < 1 || c.recoverySuccesses > 10) {
        error = L"Probe settings are outside allowed ranges.";
        return false;
    }
    if (c.heartbeatIntervalSeconds < 30 || c.heartbeatIntervalSeconds > 3600 ||
        c.heartbeatTimeoutSeconds < 3 || c.heartbeatTimeoutSeconds > 30) {
        error = L"Healthchecks interval must be 30-3600s and timeout 3-30s.";
        return false;
    }
    if (!c.healthchecksUrl.empty() && !ValidHealthchecksUrl(c.healthchecksUrl)) {
        error = L"Use only the full HTTPS check ping URL from hc-ping.com (no query or suffix).";
        return false;
    }
    if (c.healthchecksEnabled && c.healthchecksUrl.empty()) {
        error = L"Enter a Healthchecks ping URL before enabling heartbeats.";
        return false;
    }
    return true;
}

bool LoadConfig(Config& config, std::wstring& error) {
    Key key;
    const auto status = RegOpenKeyExW(HKEY_CURRENT_USER, kKey, 0, KEY_READ, &key.handle);
    if (status == ERROR_FILE_NOT_FOUND) return true;
    if (status != ERROR_SUCCESS) {
        error = L"Unable to read HKCU\\Software\\ProxyChecker.";
        return false;
    }
    Config temp;
    temp.proxyHost = ReadString(key.handle, L"ProxyHost", temp.proxyHost);
    temp.proxyPort = ReadDword(key.handle, L"ProxyPort", temp.proxyPort);
    temp.probeIntervalSeconds = ReadDword(key.handle, L"ProbeIntervalSec", temp.probeIntervalSeconds);
    temp.probeTimeoutSeconds = ReadDword(key.handle, L"ProbeTimeoutSec", temp.probeTimeoutSeconds);
    temp.downConfirmSeconds = ReadDword(key.handle, L"DownConfirmSec", temp.downConfirmSeconds);
    temp.recoverySuccesses = ReadDword(key.handle, L"RecoverySuccesses", temp.recoverySuccesses);
    temp.healthchecksEnabled = ReadDword(key.handle, L"HealthchecksEnabled", 0) == 1;
    temp.heartbeatIntervalSeconds = ReadDword(key.handle, L"HeartbeatIntervalSec", temp.heartbeatIntervalSeconds);
    temp.heartbeatTimeoutSeconds = ReadDword(key.handle, L"HeartbeatTimeoutSec", temp.heartbeatTimeoutSeconds);
    if (!ReadProtected(key.handle, temp.healthchecksUrl, error)) return false;
    if (!ValidateConfig(temp, error)) return false;
    config = std::move(temp);
    return true;
}

bool SaveConfig(const Config& config, std::wstring& error) {
    if (!ValidateConfig(config, error)) return false;
    Key key;
    DWORD unused = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr, 0,
        KEY_SET_VALUE, nullptr, &key.handle, &unused) != ERROR_SUCCESS) {
        error = L"Cannot open ProxyChecker settings registry key for writing.";
        return false;
    }
    // Commit the credential first. The enable flag is written last.
    const bool ok = WriteProtected(key.handle, config.healthchecksUrl) &&
        WriteString(key.handle, L"ProxyHost", config.proxyHost) &&
        WriteDword(key.handle, L"ProxyPort", config.proxyPort) &&
        WriteDword(key.handle, L"ProbeIntervalSec", config.probeIntervalSeconds) &&
        WriteDword(key.handle, L"ProbeTimeoutSec", config.probeTimeoutSeconds) &&
        WriteDword(key.handle, L"DownConfirmSec", config.downConfirmSeconds) &&
        WriteDword(key.handle, L"RecoverySuccesses", config.recoverySuccesses) &&
        WriteDword(key.handle, L"HeartbeatIntervalSec", config.heartbeatIntervalSeconds) &&
        WriteDword(key.handle, L"HeartbeatTimeoutSec", config.heartbeatTimeoutSeconds) &&
        WriteDword(key.handle, L"HealthchecksEnabled", config.healthchecksEnabled ? 1UL : 0UL);
    if (!ok) error = L"Unable to save one or more registry settings.";
    return ok;
}

}  // namespace proxychecker
