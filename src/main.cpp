#include "engine.hpp"
#include "settings.hpp"

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <memory>
#include <string>

namespace {
using namespace proxychecker;

constexpr wchar_t kAppClass[] = L"ProxyChecker.MainWindow";
constexpr wchar_t kSettingsClass[] = L"ProxyChecker.SettingsWindow";
constexpr UINT kTrayMessage = WM_APP + 29;
constexpr UINT kTrayId = 42;
constexpr UINT kMenuOpen = 500;
constexpr UINT kMenuDiagnose = 501;
constexpr UINT kMenuQuit = 502;

enum ControlId : int {
    MainStatus = 100,
    MainProxy = 101,
    MainLastProbe = 102,
    MainLastState = 103,
    MainHealthchecks = 104,
    MainDetails = 105,
    ButtonDiagnose = 106,
    ButtonSettings = 107,
    ButtonLogs = 108,
    ButtonHide = 109,
    SettingProxyHost = 200,
    SettingProxyPort = 201,
    SettingProbeInterval = 202,
    SettingProbeTimeout = 203,
    SettingDownConfirm = 204,
    SettingRecovery = 205,
    SettingHealthEnabled = 206,
    SettingHealthUrl = 207,
    SettingHealthClear = 208,
    SettingHeartbeatInterval = 209,
    SettingHeartbeatTimeout = 210,
    SettingSave = 211,
    SettingCancel = 212
};

HINSTANCE gInstance = nullptr;
HWND gMain = nullptr;
HWND gSettings = nullptr;
HWND gStatus = nullptr;
HFONT gFont = nullptr;
HFONT gLargeFont = nullptr;
std::unique_ptr<Engine> gEngine;
Config gConfig;
bool gTrayInstalled = false;

HWND AddControl(HWND parent, const wchar_t* type, const wchar_t* text,
    DWORD style, int x, int y, int w, int h, int id = 0,
    DWORD exStyle = 0, bool large = false) {
    HWND child = CreateWindowExW(exStyle, type, text, style | WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), gInstance, nullptr);
    if (child) SendMessageW(child, WM_SETFONT,
        reinterpret_cast<WPARAM>(large ? gLargeFont : gFont), TRUE);
    return child;
}

void Label(HWND parent, const wchar_t* value, int x, int y, int w, int h,
    int id = 0, bool large = false) {
    AddControl(parent, L"STATIC", value, SS_LEFT, x, y, w, h, id, 0, large);
}

void Group(HWND parent, const wchar_t* title, int x, int y, int w, int h) {
    AddControl(parent, L"BUTTON", title, BS_GROUPBOX, x, y, w, h);
}

void Button(HWND parent, const wchar_t* title, int x, int y, int w, int h, int id) {
    AddControl(parent, L"BUTTON", title, BS_PUSHBUTTON | WS_TABSTOP,
        x, y, w, h, id);
}

void Edit(HWND parent, int id, const std::wstring& value,
    int x, int y, int w, int h, bool password = false) {
    AddControl(parent, L"EDIT", value.c_str(), WS_TABSTOP | ES_AUTOHSCROLL |
        (password ? ES_PASSWORD : 0), x, y, w, h, id, WS_EX_CLIENTEDGE);
}

std::wstring TextOf(HWND parent, int id) {
    wchar_t buffer[2048]{};
    GetDlgItemTextW(parent, id, buffer, static_cast<int>(_countof(buffer)));
    return buffer;
}

void SetText(HWND parent, int id, const std::wstring& text) {
    SetDlgItemTextW(parent, id, text.c_str());
}

std::wstring Number(DWORD n) { return std::to_wstring(n); }

bool ReadNumber(HWND parent, int id, DWORD& dest) {
    BOOL valid = FALSE;
    const UINT value = GetDlgItemInt(parent, id, &valid, FALSE);
    if (!valid) {
        MessageBoxW(parent, L"Enter a valid whole number in every numeric field.",
            L"ProxyChecker - Settings", MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(parent, id));
        return false;
    }
    dest = value;
    return true;
}

void IconForState(Status state) {
    if (!gTrayInstalled) return;
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = gMain;
    data.uID = kTrayId;
    data.uFlags = NIF_ICON | NIF_TIP;
    const wchar_t* icon = state == Status::Up ? IDI_INFORMATION :
        state == Status::Down ? IDI_ERROR : IDI_WARNING;
    data.hIcon = LoadIconW(nullptr, icon);
    swprintf_s(data.szTip, L"ProxyChecker - %s", StatusName(state));
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void CreateTray() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = gMain;
    data.uID = kTrayId;
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = kTrayMessage;
    data.hIcon = LoadIconW(nullptr, IDI_WARNING);
    wcscpy_s(data.szTip, L"ProxyChecker - UNKNOWN");
    gTrayInstalled = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    if (gTrayInstalled) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
    }
}

void RemoveTray() {
    if (!gTrayInstalled) return;
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = gMain;
    data.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &data);
    gTrayInstalled = false;
}

void ShowMain() {
    ShowWindow(gMain, SW_SHOW);
    ShowWindow(gMain, SW_RESTORE);
    SetForegroundWindow(gMain);
}

void RefreshDashboard() {
    if (!gMain || !gEngine) return;
    const auto s = gEngine->GetSnapshot();
    SetText(gMain, MainStatus, StatusName(s.state));
    SetText(gMain, MainProxy,
        gConfig.proxyHost + L":" + Number(gConfig.proxyPort));
    SetText(gMain, MainLastProbe, s.lastProbe);
    SetText(gMain, MainLastState, s.lastTransition);
    SetText(gMain, MainHealthchecks,
        (s.healthchecksEnabled ? L"Enabled  |  " : L"Disabled  |  ") +
        s.lastHeartbeat);
    SetText(gMain, MainDetails, s.diagnostics);
    IconForState(s.state);
    InvalidateRect(gStatus, nullptr, TRUE);
}

void OpenLogs() {
    const auto folder = LogFolder();
    CreateDirectoryW((folder.substr(0, folder.find_last_of(L'\\'))).c_str(), nullptr);
    CreateDirectoryW(folder.c_str(), nullptr);
    ShellExecuteW(gMain, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void ExitApplication() {
    if (gSettings) DestroyWindow(gSettings);
    RemoveTray();
    if (gEngine) gEngine->Stop();
    DestroyWindow(gMain);
}

void SaveSettings(HWND window) {
    Config next = gConfig;
    next.proxyHost = TextOf(window, SettingProxyHost);
    if (!ReadNumber(window, SettingProxyPort, next.proxyPort) ||
        !ReadNumber(window, SettingProbeInterval, next.probeIntervalSeconds) ||
        !ReadNumber(window, SettingProbeTimeout, next.probeTimeoutSeconds) ||
        !ReadNumber(window, SettingDownConfirm, next.downConfirmSeconds) ||
        !ReadNumber(window, SettingRecovery, next.recoverySuccesses) ||
        !ReadNumber(window, SettingHeartbeatInterval, next.heartbeatIntervalSeconds) ||
        !ReadNumber(window, SettingHeartbeatTimeout, next.heartbeatTimeoutSeconds))
        return;

    next.healthchecksEnabled =
        SendDlgItemMessageW(window, SettingHealthEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const bool clear = SendDlgItemMessageW(window,
        SettingHealthClear, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (clear) next.healthchecksUrl.clear();
    else {
        const auto supplied = TextOf(window, SettingHealthUrl);
        if (!supplied.empty()) next.healthchecksUrl = supplied;
    }
    std::wstring error;
    if (!ValidateConfig(next, error) || !SaveConfig(next, error)) {
        MessageBoxW(window, error.c_str(), L"ProxyChecker - Invalid settings",
            MB_OK | MB_ICONERROR);
        return;
    }

    gConfig = std::move(next);
    if (gEngine) gEngine->UpdateConfig(gConfig);
    LogEvent(L"Configuration updated (secret values omitted)");
    DestroyWindow(window);
    RefreshDashboard();
}

LRESULT CALLBACK SettingsProc(HWND window, UINT msg, WPARAM wp, LPARAM /*lp*/) {
    switch (msg) {
    case WM_CREATE: {
        Label(window, L"All settings are stored in HKCU\\Software\\ProxyChecker.", 20, 12, 730, 24);
        Group(window, L"HTTP proxy", 20, 42, 710, 100);
        Label(window, L"Host or IP", 38, 67, 360, 20);
        Edit(window, SettingProxyHost, gConfig.proxyHost, 38, 88, 430, 26);
        Label(window, L"Port", 490, 67, 130, 20);
        Edit(window, SettingProxyPort, Number(gConfig.proxyPort), 490, 88, 110, 26);

        Group(window, L"Route monitoring", 20, 155, 710, 150);
        Label(window, L"Probe interval (5-300 s)", 38, 181, 195, 22);
        Label(window, L"Timeout per endpoint (2-30 s)", 260, 181, 225, 22);
        Label(window, L"DOWN after (30-3600 s)", 506, 181, 190, 22);
        Edit(window, SettingProbeInterval, Number(gConfig.probeIntervalSeconds), 38, 207, 110, 26);
        Edit(window, SettingProbeTimeout, Number(gConfig.probeTimeoutSeconds), 260, 207, 110, 26);
        Edit(window, SettingDownConfirm, Number(gConfig.downConfirmSeconds), 506, 207, 110, 26);
        Label(window, L"Successful probes required to recover (1-10)", 38, 249, 370, 24);
        Edit(window, SettingRecovery, Number(gConfig.recoverySuccesses), 430, 247, 110, 26);

        Group(window, L"Healthchecks.io (independent Telegram DOWN/UP alerts)", 20, 317, 710, 202);
        AddControl(window, L"BUTTON", L"Send heartbeats after confirmed UP",
            BS_AUTOCHECKBOX | WS_TABSTOP, 38, 343, 480, 26, SettingHealthEnabled);
        if (gConfig.healthchecksEnabled)
            SendDlgItemMessageW(window, SettingHealthEnabled, BM_SETCHECK, BST_CHECKED, 0);
        Label(window, L"Healthchecks Ping URL (https://hc-ping.com/...)",
            38, 375, 510, 21);
        Edit(window, SettingHealthUrl, L"", 38, 399, 660, 26, true);
        Label(window, gConfig.healthchecksUrl.empty() ?
            L"No URL stored. Enter full Ping URL to enable." :
            L"URL saved securely using Windows DPAPI. Leave blank to keep it.",
            38, 430, 600, 20);
        AddControl(window, L"BUTTON", L"Clear stored URL", BS_AUTOCHECKBOX | WS_TABSTOP,
            560, 344, 150, 24, SettingHealthClear);
        Label(window, L"Heartbeat interval (30-3600 s)", 38, 456, 250, 20);
        Label(window, L"Heartbeat timeout (3-30 s)", 360, 456, 270, 20);
        Edit(window, SettingHeartbeatInterval,
            Number(gConfig.heartbeatIntervalSeconds), 38, 480, 110, 26);
        Edit(window, SettingHeartbeatTimeout,
            Number(gConfig.heartbeatTimeoutSeconds), 360, 480, 110, 26);

        Label(window, L"Safety: heartbeats use the configured proxy only; no direct fallback.",
            20, 528, 710, 24);
        Button(window, L"Save settings", 465, 562, 140, 34, SettingSave);
        Button(window, L"Cancel", 615, 562, 114, 34, SettingCancel);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == SettingSave) { SaveSettings(window); return 0; }
        if (LOWORD(wp) == SettingCancel) { DestroyWindow(window); return 0; }
        break;
    case WM_CLOSE:
        DestroyWindow(window); return 0;
    case WM_DESTROY:
        if (gSettings == window) gSettings = nullptr;
        return 0;
    default: break;
    }
    return DefWindowProcW(window, msg, wp, 0);
}

void OpenSettings() {
    if (gSettings) {
        SetForegroundWindow(gSettings);
        return;
    }
    gSettings = CreateWindowExW(WS_EX_DLGMODALFRAME, kSettingsClass,
        L"ProxyChecker - Settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 770, 648, gMain, nullptr, gInstance, nullptr);
    if (gSettings) {
        ShowWindow(gSettings, SW_SHOWNORMAL);
        UpdateWindow(gSettings);
    }
}

void ShowTrayMenu() {
    POINT cursor{};
    GetCursorPos(&cursor);
    const HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, kMenuOpen, L"Open ProxyChecker");
    AppendMenuW(menu, MF_STRING, kMenuDiagnose, L"Diagnose");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuQuit, L"Exit");
    SetForegroundWindow(gMain);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, gMain, nullptr);
    DestroyMenu(menu);
}

LRESULT CALLBACK MainProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        Group(window, L"Connection", 20, 15, 790, 180);
        Label(window, L"Current proxy-route state", 42, 43, 320, 24);
        gStatus = AddControl(window, L"STATIC", L"UNKNOWN", SS_LEFT,
            42, 69, 300, 42, MainStatus, 0, true);
        Label(window, L"Proxy:", 410, 52, 80, 22);
        Label(window, L"", 492, 52, 290, 25, MainProxy);
        Label(window, L"Last probe:", 42, 123, 96, 21);
        Label(window, L"", 142, 123, 635, 42, MainLastProbe);
        Label(window, L"Transition:", 42, 161, 95, 22);
        Label(window, L"", 142, 161, 635, 22, MainLastState);
        Group(window, L"Healthchecks.io", 20, 205, 790, 88);
        Label(window, L"", 42, 238, 740, 44, MainHealthchecks);
        Group(window, L"Diagnostics", 20, 304, 790, 229);
        AddControl(window, L"EDIT", L"Click Diagnose to test all endpoints.",
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
            42, 336, 747, 175, MainDetails, WS_EX_CLIENTEDGE);
        Button(window, L"Diagnose", 20, 546, 138, 37, ButtonDiagnose);
        Button(window, L"Settings", 172, 546, 138, 37, ButtonSettings);
        Button(window, L"Open logs", 324, 546, 138, 37, ButtonLogs);
        Button(window, L"Hide to tray", 476, 546, 140, 37, ButtonHide);
        CreateTray();
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ButtonDiagnose: case kMenuDiagnose:
            if (gEngine) gEngine->RequestDiagnostics();
            return 0;
        case ButtonSettings: OpenSettings(); return 0;
        case ButtonLogs: OpenLogs(); return 0;
        case ButtonHide: ShowWindow(window, SW_HIDE); return 0;
        case kMenuOpen: ShowMain(); return 0;
        case kMenuQuit: ExitApplication(); return 0;
        default: break;
        }
        break;
    case WM_CTLCOLORSTATIC:
        if (reinterpret_cast<HWND>(lp) == gStatus) {
            const auto state = gEngine ? gEngine->GetSnapshot().state : Status::Unknown;
            const COLORREF color = state == Status::Up ? RGB(0, 113, 62) :
                state == Status::Down ? RGB(177, 31, 32) :
                state == Status::Suspect ? RGB(154, 92, 0) : RGB(82, 92, 102);
            SetTextColor(reinterpret_cast<HDC>(wp), color);
            SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        }
        break;
    case WM_PROXYCHECKER_UPDATE:
        RefreshDashboard(); return 0;
    case kTrayMessage:
        // v4 tray callback messages have LOWORD(lParam) as the event ID.
        if (LOWORD(lp) == WM_LBUTTONDBLCLK) ShowMain();
        if (LOWORD(lp) == WM_CONTEXTMENU || LOWORD(lp) == WM_RBUTTONUP)
            ShowTrayMenu();
        return 0;
    case WM_CLOSE:
        ShowWindow(window, SW_HIDE); return 0;
    case WM_DESTROY:
        RemoveTray();
        PostQuitMessage(0);
        return 0;
    default: break;
    }
    return DefWindowProcW(window, msg, wp, lp);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    gInstance = instance;
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\ProxyChecker.Instance");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"ProxyChecker is already running.",
            L"ProxyChecker", MB_OK | MB_ICONINFORMATION);
        CloseHandle(mutex);
        return 0;
    }

    std::wstring error;
    if (!LoadConfig(gConfig, error)) {
        MessageBoxW(nullptr,
            (L"Cannot load settings: " + error +
             L"\r\n\r\nMonitoring will start with safe defaults and Healthchecks disabled."
            ).c_str(), L"ProxyChecker settings", MB_OK | MB_ICONWARNING);
        gConfig = Config{};
    }

    INITCOMMONCONTROLSEX init{sizeof(init), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&init);
    gFont = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    gLargeFont = CreateFontW(-31, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    wc.lpfnWndProc = MainProc;
    wc.lpszClassName = kAppClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_INFORMATION);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    if (!RegisterClassExW(&wc)) return 2;
    wc.lpfnWndProc = SettingsProc;
    wc.lpszClassName = kSettingsClass;
    if (!RegisterClassExW(&wc)) return 3;

    gMain = CreateWindowExW(0, kAppClass, L"ProxyChecker - Proxy Route Monitor",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 850, 637,
        nullptr, nullptr, instance, nullptr);
    if (!gMain) return 4;

    gEngine = std::make_unique<Engine>(gMain, gConfig);
    gEngine->Start();
    ShowWindow(gMain, showCommand);
    UpdateWindow(gMain);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (gSettings && IsDialogMessageW(gSettings, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (gEngine) gEngine->Stop();
    gEngine.reset();
    if (gFont) DeleteObject(gFont);
    if (gLargeFont) DeleteObject(gLargeFont);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return static_cast<int>(msg.wParam);
}
