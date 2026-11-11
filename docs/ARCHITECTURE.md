# Architecture contract

## Modules (planned)

1. **Application/Win32 UI**: message loop, system tray, settings tabs, diagnostics, status history; never block UI thread during network I/O.
2. **Settings/Registry**: typed values under `HKCU\Software\ProxyChecker`; validate values before persistence; transactional-ish safe defaults. DPAPI CurrentUser for Healthchecks ping URL and any secret.
3. **Probe transport (WinHTTP)**: explicit per-request HTTP proxy, connect/receive/overall timeout, proper cancellation, TLS verification, redirects, HTTP status, transport error code and latency; do not count HTTP CONNECT acceptance alone as success.
4. **State machine**: `UNKNOWN`, `UP`, `SUSPECT`, `DOWN`. Any valid external HTTPS success means route reachable. Default DOWN confirmation 90 seconds of continuous failure of all endpoints; recovery requires two successes. No per-request notification spam.
5. **Healthchecks client**: periodic heartbeat through monitored proxy only when route is confirmed UP. Suppress heartbeat on DOWN/SUSPECT/UNKNOWN. Independent Healthchecks.io service notifies Telegram when expected pings cease. Never send through a silent direct fallback.
6. **Diagnostics/logging**: separate time-stamped per-endpoint diagnostics and rolling log files under `%LOCALAPPDATA%\\ProxyChecker\\Logs`. Sanitize all sensitive data (especially Healthchecks URL).

## Editable configuration (all in HKCU registry)

- Proxy: host and port, default `172.16.136.19:3128` (change at runtime).
- Probes: targets, interval, per-endpoint timeout, DOWN confirmation and recovery threshold.
- Healthchecks: enabled, DPAPI-protected ping URL, interval, timeout, send-through-proxy option (default true; changing it must show a warning).
- UX: startup preference, minimize-to-tray, notifications and window layout.

## Reliability invariants

- First-run never invents an UP/healthy heartbeat before a real successful probe.
- One endpoint down does not imply VLESS down.
- No Healthchecks ping if the observed proxy route is unverified or not healthy.
- Never infer the state of the VLESS daemon itself from remote HTTPS success alone. The observed guarantee is proxy-route reachability, assuming OpenWrt/Podkop has no direct fallback.
- Preserve accurate failure detail without exposing tokens in logs or telemetry.
- After a restart, an UNKNOWN state must not generate an unearned recovery notification.
- Stopping the app results in missing heartbeats (external alert); user must be made aware of this behavior.
