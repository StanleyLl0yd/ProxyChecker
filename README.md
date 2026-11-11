# ProxyChecker

Native, standalone Windows proxy-route monitor. Planned replacement for the `Watch-VLESS.ps1` PowerShell monitor.

> **Status: M0 bootstrap / native build foundation only.** Monitoring, tray, registry settings and Healthchecks heartbeats are specified but **not yet implemented**. Do not use the prototype as a production monitor.

## Requirements

- Native **C++23 / Win32**, portable single `ProxyChecker.exe`, Windows x64, MSVC **static CRT `/MT`**, no .NET, Qt, Electron, Python, PowerShell or additional redistributables. Windows system DLLs are allowed.
- Native Windows UI with tray status; visible network errors, detailed on-demand diagnostics and daily logs.
- Editable HTTP proxy address (default `172.16.136.19:3128`), persisted between launches.
- Probe through the configured proxy using **WinHTTP** (not ICMP ping; not `curl.exe`). Four independent HTTPS targets: Cloudflare, Microsoft, Telegram and Google. One success is enough for `UP`; prevent false `DOWN` alarms with a configurable confirmation window (default 90 seconds).
- Healthchecks.io integration: editable ping URL, enabled toggle, heartbeat interval (default 60 s), timeout, proxy-routing option (default ON). Heartbeats **must stop on confirmed loss of reachability**; do not indicate healthy status when the monitored path is down.
- Healthchecks.io sends DOWN/UP notifications independently of the broken tunnel; the app does not need a local Telegram token to send DOWN alerts.
- All **configuration and persistent runtime state** in `HKCU\Software\ProxyChecker`. Healthchecks ping URL and other credentials protected with Windows DPAPI (CurrentUser); no INI/JSON/config files.
- Detailed diagnostic **logs** are files under `%LOCALAPPDATA%\\ProxyChecker\\Logs` (logs are not settings). Redact sensitive URLs, tokens and credentials.
- Graceful startup/shutdown, explicit status `UP / SUSPECT / DOWN / UNKNOWN`, no notification spam, safe failures, tests and Windows CI.

## Build (once sources are implemented)

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

This repository currently includes a **nonfunctional Win32 foundation window** to validate the toolchain and CI; it is not the finished monitor.

## Work plan

See [ROADMAP](docs/ROADMAP.md), [ARCHITECTURE](docs/ARCHITECTURE.md), [SECURITY](SECURITY.md) and [CONTRIBUTING](CONTRIBUTING.md).

## License

No license has been selected. Public source visibility does **not** automatically grant a license to reuse the code. The owner intends to switch the repository to private after creation.
