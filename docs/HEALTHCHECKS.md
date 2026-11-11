# Healthchecks.io + Telegram setup

ProxyChecker reports successful, verified proxy-route operation by sending a heartbeat through the **monitored HTTP proxy** to Healthchecks.io. If heartbeats stop, **Healthchecks.io** independently sends DOWN alerts through its own Telegram bot. It can also notify on recovery.

## 1. Create a check

1. Sign in at https://healthchecks.io/ (free Hobbyist plan is sufficient).
2. Create one check named **ProxyChecker - VLESS**. Use a *Simple* schedule:
   - **Period:** 1 minute
   - **Grace Time:** 1 minute
   - Optional slug: proxychecker-vless
   - Optional tags: proxychecker vless windows
3. Use the **UUID Ping URL** copied from the check, e.g. `https://hc-ping.com/<uuid>` (placeholder, not a real credential).

**Keep this URL secret:** possession of the URL allows anyone to forge successful heartbeats. Never put it in repository issues, screenshots, chat, git, logs or support tickets.

## 2. Connect Telegram

1. Open https://t.me/HealthchecksBot on a device with independent Telegram access.
2. Send `/start`. For groups, add the bot and send `/start@HealthchecksBot`.
3. Follow the confirmation link, choose the Healthchecks.io project, select **Connect Telegram**.
4. In the Healthchecks dashboard, confirm this integration is **enabled for the ProxyChecker check**.

HealthchecksBot is separate from the ServiceDesk Notifier bot. Do not store a Telegram bot token in ProxyChecker.

## 3. Configure ProxyChecker

Open **Settings**:

- Proxy: default `172.16.136.19:3128`, editable.
- Probe interval: 10 s, timeout per endpoint: 12 s, confirm DOWN: 90 s, recovery: 2 successes.
- Healthchecks: check **Send heartbeats after confirmed UP**, enter the full UUID ping URL, interval **60 s**, timeout **10 s**, and **Save settings**.
- When editing settings later, leave the URL input blank to **keep the encrypted current value**. Tick **Clear stored URL** to remove it.

All settings are in `HKCU\Software\ProxyChecker`; ping URL is DPAPI CurrentUser encrypted. No settings file is created. Logs are under `%LOCALAPPDATA%\ProxyChecker\Logs` and must **not** contain the URL.

Only when at least one external HTTPS probe succeeds and the state machine confirms UP will ProxyChecker send a heartbeat. No direct-network fallback is used.

## 4. Test the complete path

1. Keep the Windows PC awake and run ProxyChecker. Wait for **UP**, and **Healthchecks accepted heartbeat**.
2. Healthchecks UI should show the check as **Up** and display a recent ping.
3. **Exit the program fully via the tray menu** (or intentionally interrupt the proxy while keeping the app running).
4. After the last heartbeat passes Period + Grace, expect a Telegram **DOWN** notification. It may take a little longer to arrive.
5. Relaunch ProxyChecker (or restore the proxy). Wait for two successful proxy probes; the next accepted heartbeat changes the check to **UP**, triggering an external recovery notification.

The external service also reports missed heartbeats if **Windows sleeps, the program exits, the computer is disconnected, or the proxy fails**. It does not independently distinguish these reasons.

**Do not test a heartbeat by opening its Ping URL in a browser:** that manually creates a success and can mask a real outage.

## 5. Maintenance and security

- When using the *Simple* schedule, Healthchecks declares a check Late after Period and Down after Period + Grace.
- Use at most 5 pings/minute per check. The default 1/minute is safe.
- No `/fail` or `/start` endpoints are used, only the base success endpoint.
- Rotating the ping URL requires updating the protected URL in settings.
- Telegram alerts require an internet connection on the recipient's device independent of the failed tunnel.

Official references:
- https://healthchecks.io/docs/configuring_checks/
- https://healthchecks.io/docs/http_api/
- https://healthchecks.io/integrations/telegram/
