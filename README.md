# Rubato

> **When AI thinks, you move.**

[中文说明](README_zh.md) | English

![Rubato](assets/product.jpg)

Rubato is an ESP8266 desk companion with a 240×240 color display. It watches your AI coding sessions and turns AI wait time into health breaks.

## How it works

1. A small PC plugin publishes your AI session state over MQTT (TLS): `thinking` → `generating` → `done`
2. A breathing orb mirrors the state — cream while thinking, glacier blue while generating, green when done
3. When a task looks long (est. ≥ 30 s), Rubato takes over the full screen with a colorful micro-break reminder

## Six daily breaks

Water · Bathroom break · Eye rest · Neck stretch · Kegels · Standing desk

Each activity has a daily quota, and two full-screen breaks stay at least 30 minutes apart, so reminders land at a humane rhythm — never spammy. The promise is reminders delivered, not completion-rate check-ins.

## More than reminders

- **Desk clock** — NTP time, western date format, weather with auto-location (Open-Meteo)
- **State orb** — per-device MQTT credentials; Rubato mirrors the session state, never the message text
- **OTA self-update** — segmented HTTPS download with an anti-brick guard: a bad update lands in Safe Mode and self-heals remotely, no USB needed
- **Web settings** — brightness, orientation, °C/°F from your browser; everything survives power loss

## Hardware & source

ESP8266 (NodeMCU) + 240×240 TFT, Arduino framework (ESP8266 core 3.1.2, TFT_eSPI). Firmware in `rubato.ino`, health-icon pipeline in `tools/`. Flash, provision WiFi through the built-in hotspot, pair with the broker via one serial command — done.

## License

GPL-3.0 — original clock by Misaka (2021), redesigned as Rubato.

Full development history in [changelog.md](changelog.md).
