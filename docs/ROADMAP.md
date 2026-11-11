# Development roadmap

- [ ] **M0**: create repository, establish buildable MSVC Win32/C++23 scaffold, Windows CI, protected `main`, initial docs; accept after CI succeeds.
- [ ] **M1**: native tray UI and settings screens; editable proxy; `HKCU` typed settings; robust shutdown and single-instance handling.
- [ ] **M2**: WinHTTP multi-endpoint probes; explicit error diagnostics, logs, robust state machine; actual route-down/recovery tests.
- [ ] **M3**: Healthchecks.io integration; DPAPI storage; heartbeat only through healthy monitored path; DOWN/UP end-to-end tests.
- [ ] **M4**: release build `/MT`, icon/UX polish, autostart preference, test coverage, portable EXE release and acceptance.

## MVP acceptance

1. Change proxy from UI, restart app, verify setting persists (no external config files).
2. Probes use configured proxy and behave correctly with healthy route plus one failing endpoint.
3. Simulate VLESS loss; suppress heartbeats; Healthchecks independently sends Telegram DOWN (without tunnel).
4. Restore route; resume heartbeats; Healthchecks independently sends Telegram UP; avoid duplicate recovery notices.
5. App never writes secrets to repository, logs or command line.
6. Executable runs on clean supported Windows x64 without .NET or VC++ redistributable.
