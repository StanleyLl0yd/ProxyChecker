# Development roadmap

- [x] **M0 — build foundation:** public repo created, initial MSVC C++23 Win32 scaffold compiled in GitHub Actions, active protected `main`, baseline documentation.
- [x] **M1 — implemented (PR #2):** Win32 dashboard, system tray, editable proxy, all settings in HKCU, DPAPI for Healthchecks URL, single-instance; **Windows GUI acceptance pending**.
- [x] **M2 — implemented (PR #2):** WinHTTP four-target HTTPS probes, explicit errors/latency, 90s outage confirmation, two-success recovery, local logs, state tests; **real proxy failure/recovery acceptance pending**.
- [x] **M3 — implemented (PR #2):** Healthchecks.io heartbeats over monitored proxy, disabled until configured, verified `200 OK`, only after confirmed UP, fail closed on missing check URL; **real Healthchecks Telegram DOWN/UP acceptance pending**.
- [ ] **M4 — release preparation:** icon/UX polish, optional autostart, integration tests, security review, portable EXE versioned release and final acceptance.

Checkboxes in M1–M3 mean implementation complete, **not accepted**. Acceptance requires physical Windows/OpenWrt/Healthchecks test.

## Acceptance checks

1. Change proxy in Settings, close and reopen app, verify new address persisted under HKCU.
2. Diagnose all four external HTTPS targets; verify statuses, latency and WinHTTP error codes in `%LOCALAPPDATA%\ProxyChecker\Logs`.
3. When at least one target works, state becomes UP after two successful probe cycles.
4. With all endpoints unreachable, status becomes SUSPECT and eventually DOWN after at least 90 seconds of uninterrupted failed cycles. No false DOWN on one failed endpoint.
5. Create Healthchecks.io check and Telegram integration per `docs/HEALTHCHECKS.md`. An UP ProxyChecker sends 1 successful ping/minute over monitored proxy only; observe corresponding event in web UI.
6. Interrupt the proxy or fully Exit ProxyChecker: external Healthchecks sends DOWN alert after schedule+grace. Restore route: sends UP, without a flood of repeated messages.
7. Inspect actual executable import list on Windows to confirm static CRT, no additional runtime; confirm DPAPI secret is not present in logs or plaintext registry.
8. Re-run exact-head CI and tests before merging. No production release until owner tests the preview.
