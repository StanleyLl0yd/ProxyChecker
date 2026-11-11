# Security policy

- Report vulnerabilities via repository private vulnerability reporting (if enabled) or directly to the repository owner. Do not publicly post exploit details or credentials.
- Never commit Telegram bot tokens, chat IDs, Healthchecks ping URLs, private keys, passwords, cookies, or sensitive IP/domain inventories.
- Healthchecks ping URLs are credentials: anyone possessing one may forge a successful heartbeat. Store using DPAPI CurrentUser in `HKCU\Software\ProxyChecker`; redact from logs, exception dumps and screenshots.
- Use WinHTTP HTTPS certificate validation. Do not disable certificate verification or add an unconfirmed direct-network fallback.
- Use minimum required permissions. Monitoring and registry writes under HKCU must not require administrator rights.
- GitHub Actions must have read-only repository token by default; never expose secrets to untrusted pull requests.
- Before converting this initially public repository to private, assume all files/commits and any clones remain accessible to prior viewers. Visibility changes do not erase earlier exposure.
