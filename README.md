# Rubato

> **When AI thinks, you move.**

[中文说明](README_zh.md) | English

![Rubato](assets/product.jpg)

![Rubato in action](assets/product.gif)

Rubato is a palm-sized retro-Macintosh screen (ESP8266, 240×240 color display) built for one job: looking after the bodies of programmers and heavy AI users. Long sessions keep you pinned to the chair — dry eyes, a stiff neck, an aching back. Rubato watches your AI coding sessions and turns long waits into real, gentle breaks — water, eye rest, a stretch — on time, every day. The rest of the time it just sits quietly on your desk, never in your way.

## Get one

Official store: **[Tindie — Rubato, Retro Mac AI Desk Companion](https://www.tindie.com/products/beartificialintelligence/rubato-retro-mac-ai-desk-companion/)**

## How it works

1. A small PC plugin publishes your AI session state over MQTT (TLS): `thinking` → `generating` → `done`
2. A breathing orb mirrors the state — cream while thinking, glacier blue while generating, green when done
3. When a task looks long (est. ≥ 30 s), Rubato eases into a full-screen micro-break reminder

## Six daily breaks

Water · Bathroom break · Eye rest · Neck stretch · Kegels · Standing desk

The six targets are what sitting all day actually costs: dehydration, eye strain, a stiff neck, stagnant circulation. Each activity has a daily quota, and two full-screen breaks stay at least 30 minutes apart. The design is quiet and natural — soft colors, slow rhythm, one gentle nudge at a time — so every break feels light and pleasant, never like an interruption. The promise is reminders delivered, not completion-rate check-ins.

## More than reminders

- **Desk clock** — NTP time, western date format, weather with auto-location (Open-Meteo)
- **State orb** — per-device MQTT credentials; Rubato mirrors the session state, never the message text
- **OTA self-update** — segmented HTTPS download with an anti-brick guard: a bad update lands in Safe Mode and self-heals remotely, no USB needed
- **Web settings** — brightness, orientation, °C/°F from your browser; everything survives power loss

## Plugins

A plugin for every coding agent, all speaking the same MQTT contract — install from [Rubato_Plugins](https://github.com/lovaxi/Rubato_Plugins).

| Agent | Status |
|---|---|
| DeepSeek Harness | available |
| OpenClaw | available |
| Cursor | available |
| OpenCode | available |
| Codex | planned |
| Claude Code | planned |

## Hardware & source

ESP8266 (NodeMCU) + 240×240 TFT, powered over USB Type-C. Arduino framework (ESP8266 core 3.1.2, TFT_eSPI). Firmware in `rubato.ino`, health-icon pipeline in `tools/`. Flash, provision WiFi through the built-in hotspot, pair with the broker via one serial command — done.

## License

GPL-3.0 — original clock by Misaka (2021), redesigned as Rubato.
Hardware design scheme from [SmallDesktopDisplay](https://github.com/chuxin520922/SmallDesktopDisplay) by chuxin520922.

Full development history in [changelog.md](changelog.md).
