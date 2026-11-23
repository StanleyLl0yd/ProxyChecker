# PingZen + Telegram: free heartbeat setup

ProxyChecker sends GET heartbeat requests through the **monitored HTTP proxy** only after confirmed UP. PingZen independently detects missing pings and sends DOWN / RECOVERED alerts.

## 1. Create a free monitor

1. Register at https://pingzen.dev/ru/ with Telegram, Google, Yandex or email. No bank card is required.
2. Open **Мониторы → Создать монитор → Heartbeat**.
3. Name: `ProxyChecker-VLESS`; expected interval: **1 minute**; grace period: **1 minute**.
4. Copy the full, unique URL of the form `https://pingzen.dev/api/v1/ping/YOUR_PING_KEY/YOUR_SLUG`.
5. Leave optional `X-Secret` protection disabled: the current app uses the secret URL and does not add a custom header.

**The actual ping URL is a bearer secret.** Never place it in Git, issues, chats, logs, screenshots or support requests. Opening it in a browser sends a real heartbeat, so do not use the browser to test it.

## 2. Connect Telegram

1. In PingZen open **Оповещения → Создать оповещение → Telegram**.
2. Select **Подключить Telegram**, open https://t.me/pingzen_bot and press **Start**.
3. Select `ProxyChecker-VLESS` from the **Heartbeat monitors** section of the alert.
4. Enable **DOWN** and **RECOVERED** notifications and send a test alert; verify delivery and PingZen alert history.

## 3. Configure ProxyChecker

1. Run the updated Windows test executable and open **Settings**.
2. Confirm the monitored proxy address (default `172.16.136.19:3128`).
3. Enable **Send heartbeats after confirmed UP** and enter the entire PingZen heartbeat URL.
4. Set heartbeat interval to **60 seconds**, timeout to **10 seconds**, save.
5. When editing later, leave the URL field empty to retain the encrypted value; use **Clear stored URL** to remove it.

The URL is protected with Windows DPAPI CurrentUser under `HKCU\Software\ProxyChecker`. The registry entry `HealthchecksUrlProtected` remains named as before for backward compatibility. It may hold either provider's URL and is never logged.

## 4. Field acceptance

1. Keep Windows awake, confirm the route reaches UP after two successful probes and check that PingZen records an actual heartbeat.
2. Disable the VLESS route (or Exit ProxyChecker). After the last heartbeat's **1-minute interval + 1-minute grace**, PingZen checks every 30 seconds and should send Telegram DOWN.
3. Restore the route or relaunch the app. After two successful probes and the next heartbeat, verify Telegram RECOVERED.
4. Review `%LOCALAPPDATA%\ProxyChecker\Logs` for errors, absence of secrets and no duplicate alerts.

Missed heartbeat does not prove VLESS failed: sleeping Windows, exiting the app, or losing local internet all look identical. Also check that OpenWrt does not silently bypass VLESS through a direct route.

## Compatibility / security

- PingZen's published GET heartbeat uses HTTPS. An HTTP 2xx response is treated as an acknowledgment. Confirm the ping in PingZen history during field testing: the published API docs do not specify an exact response body.
- Legacy Healthchecks.io still requires HTTP 200 and exact response body `OK`.
- Redirects are disabled and the client cannot silently fall back to a direct network connection.

References: https://pingzen.dev/docs/heartbeats, https://pingzen.dev/docs/alerts, https://pingzen.dev/pricing.
