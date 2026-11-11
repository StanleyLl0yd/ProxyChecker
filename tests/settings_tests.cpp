#include "../src/settings.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

using namespace proxychecker;

void Require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << "\n"; std::exit(1); }
}

int main() {
    Config c;
    std::wstring error;
    Require(ValidateConfig(c, error), "valid defaults");
    Require(c.proxyHost == L"172.16.136.19" && c.proxyPort == 3128, "proxy default");
    Require(!c.healthchecksEnabled, "heartbeats disabled by default");
    Require(ValidHealthchecksUrl(L"https://hc-ping.com/00000000-0000-4000-8000-000000000000"),
            "UUID ping URL");
    Require(ValidHealthchecksUrl(L"https://hc-ping.com/abcKey/proxychecker-vless"),
            "slug-based ping URL");
    Require(!ValidHealthchecksUrl(L"http://hc-ping.com/00000000-0000-4000-8000-000000000000"),
            "requires TLS");
    Require(!ValidHealthchecksUrl(L"https://example.com/test"), "reject other domain");
    Require(!ValidHealthchecksUrl(L"https://hc-ping.com/uuid/fail"), "no /fail");
    Require(!ValidHealthchecksUrl(L"https://hc-ping.com/uuid/start"), "no /start");
    Require(!ValidHealthchecksUrl(L"https://hc-ping.com/uuid/log"), "no /log");
    Require(!ValidHealthchecksUrl(L"https://hc-ping.com/uuid?create=1"), "no query");
    Require(!ValidHealthchecksUrl(L"https://hc-ping.com/uuid/"), "no trailing slash");
    Require(!ValidHealthchecksUrl(L"https://hc-ping.com/uuid/slug/start"), "no multi suffix");
    c.healthchecksEnabled = true;
    error.clear();
    Require(!ValidateConfig(c, error), "enabled requires URL");
    c.healthchecksUrl = L"https://hc-ping.com/00000000-0000-4000-8000-000000000000";
    error.clear();
    Require(ValidateConfig(c, error), "enabled valid");
    c.proxyHost = L"example.org bad";
    error.clear();
    Require(!ValidateConfig(c, error), "invalid host");
    c.proxyHost = L"proxy.example.org";
    c.proxyPort = 65536;
    error.clear();
    Require(!ValidateConfig(c, error), "port too high");
    c.proxyPort = 8080;
    c.heartbeatIntervalSeconds = 1;
    error.clear();
    Require(!ValidateConfig(c, error), "heartbeat too frequent");
    std::cout << "settings tests passed\n";
}
