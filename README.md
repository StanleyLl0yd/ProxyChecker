# ProxyChecker

**Native portable Windows monitor for HTTP proxy routes and VLESS connectivity.**

> **Status:** M1–M3 implementation is under review. A Windows CI build is available from GitHub Actions, but this is a **test preview, not an accepted production release**. External route reachability is tested; the app does not directly inspect Podkop's internal VLESS state.

## Why

A Windows computer can connect through an OpenWrt/Podkop HTTP proxy that sends all traffic into a VLESS tunnel. When the tunnel is down, locally sending Telegram alerts through that same proxy is impossible. ProxyChecker instead sends heartbeat signals through the **verified proxy route** to Healthchecks.io; an independent Healthchecks.io server alerts Telegram if signals stop and announces recovery when they resume.

## Current capabilities (PR #2)

- Win32/C++23 UI with compact dashboard, system tray, single-instance lock and editable settings.
- Explicit HTTP proxy, default `172.16.136.19:3128`, editable at runtime.
- Four WinHTTP HTTPS targets: **Cloudflare**, **Microsoft**, **Telegram**, **Google**. They are tested in order until one succeeds; the Diagnose button tests all four. A single failed site does not mean VLESS is down.
- States `UNKNOWN`, `UP`, `SUSPECT`, `DOWN`; 90s continuous failure before DOWN; two verified successes for UP.
- Detailed HTTP status, WinHTTP error, duration, timestamp; rolling local diagnostic/event logs.
- Optional Healthchecks.io heartbeat (default every 60 seconds) using the **monitored proxy only**, sent only in confirmed UP state.
- All settings in `HKCU\Software\ProxyChecker`; Healthchecks ping URL encrypted via Windows DPAPI CurrentUser; logs in `%LOCALAPPDATA%\ProxyChecker\Logs` contain no URLs or tokens.
- Native Win32 only, MSVC /MT, no .NET, Electron, Qt, or PowerShell runtime.

## Build

Requires Windows 10/11 x64, VS 2022 with MSVC C++ tools and CMake:

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The resulting program is `build\Release\ProxyChecker.exe`. It can be run without an installer. The Windows CI workflow uploads a test executable artifact.

## Setup

1. Start the EXE, open **Settings**, enter proxy host and port if different from default, then save.
2. Press **Diagnose** to test all four external HTTPS services and inspect the log for exact errors.
3. Follow **[docs/HEALTHCHECKS.md](docs/HEALTHCHECKS.md)** to create a check and add HealthchecksBot to Telegram.
4. Enter the personal ping URL in Settings (do **not** commit or disclose it), enable heartbeats, save, and verify an accepted ping.
5. Keep the Windows computer awake; **Exit** from tray stops heartbeats and will eventually produce a DOWN alert. Hiding the window keeps monitoring active.

**Healthchecks is a remote dead-man switch:** a missing heartbeat also occurs if the PC sleeps, the program exits, or the local network is down. This does not uniquely diagnose VLESS. Do not infer a valid VLESS tunnel if OpenWrt silently falls back to a direct route.

## Roadmap and security

- [ROADMAP](docs/ROADMAP.md)
- [ARCHITECTURE](docs/ARCHITECTURE.md)
- [Healthchecks setup](docs/HEALTHCHECKS.md)
- [SECURITY](SECURITY.md)
- [CONTRIBUTING](CONTRIBUTING.md)

**License:** No license selected. Public visibility is not permission to republish or distribute the source. The owner intends to change repository visibility to Private.
