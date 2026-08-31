# Rubato — Compact Desktop Companion

> **When AI thinks, you move.**

[简体中文](README.md) | English

An ESP8266-based smart desk companion (240×240 TFT): it watches the Thinking / Generating / Done states of your AI coding sessions and turns AI wait time into human health time — drink water, take a bathroom break, rest your eyes, unwind your neck, do your Kegels, stand up and work. English UI, built for Western users.

Current version: **V1.1.1** (release number, mirrors `#define Version` in the source)

---

## 1. Positioning

Rubato is not an AI status monitor, and not another habit-tracker app:

> **AI Workload → Free Time → Best Human Action**

While AI works for you, Rubato takes care of you. The honest product promise is **reminders delivered** — no completion-rate check-ins (the device cannot verify whether you actually complied; data without a feedback loop is fake data).

---

## 2. Features

- **Desk clock (default state)**: NTP time (UTC+8, three-server failover: pool.ntp.org / ntp.aliyun.com / time.cloudflare.com), HH:MM with blinking colon, Western date format `Wed, Aug 14`; weather icon / temperature / humidity / city name (Open-Meteo, IP-based auto-location, 10-minute refresh). The degree symbol is **synthetically rendered** (digit + hand-drawn dot + unit letter) — the bundled fonts are pure ASCII with no °/℃/℉ glyphs (verified, see changelog V1.4.9)
- **AI status breathing orb**: connects to EMQX Cloud (managed MQTT over TLS), one identity per device (deviceId + token). On receiving thinking/generating it first fades in a model-name caption (~1.8s), then the orb breathes in the state color — Thinking = cream, slow breath / Generating = glacier blue, fast breath, 600ms crossfade between states; done ends with a three-stage green finale (turn green 0.5s → hold at full brightness 1.45s → fade out 0.65s), the date fading back in over 600ms. Plain-text messages never reach the screen
- **Health reminder page (V1.2, expanded to 6 activities in V1.0.4)**: on a long task the reminder **takes over the entire screen**, showing a full-color icon (100×100) + two lines of all-bold English copy; done restores the screen
- **Boot splash (V1.4.9 layout)**: Rubato wordmark (bold, upper-middle) + progress bar + `Connecting to WiFi......` + bottom nameplate row (deviceId centered in grey / version in the bottom-right); after connect success or failure the status screen takes over the wordmark area
- **OTA anti-brick guard (V1.4.9)**: RTC boot counter — an OTA arms the guard → if the new firmware runs healthy for 60s it disarms automatically → **3 consecutive armed boots without disarming = Safe Mode** (clock + NTP + OTA listener only; high-risk subsystems skipped); a fixed image can be pushed via OTA for remote self-healing, no USB needed
- **Web provisioning (WiFiManager)**: on first use or connect failure the device opens the `rubato` hotspot for WiFi and parameter setup (English UI)
- **Web admin server (always on)**: browse to the device IP to change brightness, screen orientation, temperature unit (°C/°F, with converted display) — no recompile needed
- **Persistence**: brightness / orientation / temperature unit / city / WiFi / today's reminder counts all live in EEPROM and survive power loss
- **Serial debug**: 115200, see "Usage"

---

## 3. Behavior Specification

### UI design principle

240×240 pixels — no dashboards:

> **One screen, one message.**

Large type, large icons, minimal text; simple animation, automatic transitions; understandable at a glance.

### Visual grammar (final in V1.1, reused on every page)

| Element | Rule |
|---------|------|
| Background | pure black `(0,0,0)`; no other base color anywhere |
| State colors | Thinking = cream `(255,216,168)` / Generating = glacier blue `(150,195,240)` / done hold = vivid green `(88,228,128)` |
| Ink scale | three neutral greys (city/date/temp-humidity) + warm white body text; cold colors only for Generating, green only for done |
| Motion language | every transition is a `lerpColor` fade (~600ms); breathing = asymmetric two-segment cosine (inhale 40% / exhale 60%) |

### During AI work: the two-page model (final in V1.2)

The message band (the 240×60 strip at the bottom) switches between two pages:

**Page one: breathing orb (default)** — after the caption finishes, the orb breathes in the state color with no persistent text; **short tasks (below threshold) always stay here**.

**Page two: health reminder (long-task escalation)** — when triggered it **takes over the whole screen** (clock/weather/city/date all fade out — the single exception to "the message band only handles messages"):

```text
      [icon 100x100]
   Do  TEN  Slow Kegels
```

- Full-color icon (Fluent Emoji flat art + a hand-drawn flat eye, 100×100), centered slightly high, steady — never blinking
- Copy = **two short lines** (`FreeSansBold12pt7b`, all bold; the quantity/duration word highlighted in gold at ~78% brightness to avoid glare) — a static poster, no frame animation. Quiet is elegant
- Fade out/in is done via **backlight PWM dimming** (avoiding heap risk on the 240×240 canvas): content switches while dark
- Endings: orb page done → green three-stage finale → date fades in; reminder page done → whole-screen fade → old page restored in the dark → backlight ramps back up

### Trigger rules (ALL must hold)

```text
estSec > 45 s (carried in the thinking/generating message)
AND inside work hours 9:00–18:30 (tunable)
AND ≥ 40 minutes since the last reminder
AND today's quota for the activity not exhausted (water 4 / toilet 2 / eyes 2 / neck 2 / kegel 2 / stand 1)
→ pick one activity at random from those with remaining quota
```

- Interval math: total quota 13/day (after adding the 6th activity "standing desk"; user-tuned 4/2/2/2/2/1) over 9.5h; the 40-minute global minimum gap remains the pacing constraint (theoretical 9.5h ceiling ~14 reminders — quota 13 sits just under it); per-activity pacing emerges from random picking + quotas
- `state=Estimate` only books the estimate, it never drives the state machine (it arrives before the task state and is never followed by a done — this prevents an orb hanging mid-breath)
- Latch: **at most one reminder per task**; once the reminder page is up, the task never returns to the orb page
- estSec is a prediction and will be wrong: cross-task residue is cleared at task start/done; a fallback trigger on actual elapsed wait time is a possible future enhancement

---

## 4. Hardware & Pins

| Peripheral | Pin |
|------------|-----|
| Display SCK | GPIO14 |
| Display MOSI | GPIO13 |
| Display RES | GPIO2 |
| Display DC | GPIO0 |
| Backlight LCDBL | GPIO5 |

TFT driver configuration lives in the TFT_eSPI library's `User_Setup.h`.

> **Backlight polarity**: this panel's backlight is **active-low** (higher duty = dimmer; normalized by the sketch's `blWrite()`). `User_Setup.h` must **NOT define `TFT_BACKLIGHT_ON`** — if defined, `tft.begin()` drives D1 high = screen off (the 3.x core's PWM range is 0-255; the sketch keeps the 0-1023 convention internally).

---

## 5. Usage

1. Flash the firmware and power on; it auto-connects to saved WiFi; on failure or first use it opens the `rubato` hotspot
2. Join the hotspot to finish provisioning; the device IP appears in the serial log — browse to `http://<device IP>` for the settings page
3. Change parameters later via the web page or serial commands

### Serial commands (115200)

| Command | Description |
|---------|-------------|
| `0x01` | Set brightness (0-100) |
| `0x02` | Set city name (English, e.g. Changsha) |
| `0x03` | Set screen orientation (0-3) |
| `0x04` | Reset WiFi and reboot |
| `0x05` | Random health-reminder rehearsal (counted normally) |
| `0x05 1~6` | Rehearse a specific activity (display test only, not counted): 1 water / 2 toilet / 3 eyes / 4 neck / 5 kegel / 6 stand |
| `0x06 <id> <token>` | Per-device credential write: stores deviceId + token and reboots (done before shipping) |
| `0x07` | Erase device identity and reboot (returns/RMA) |
| `done` | Same as MQTT done — ends the rehearsal and restores the screen |

### EEPROM map (1KB sector)

| Address | Content |
|---------|---------|
| 1 | Backlight brightness (1-100) |
| 2 | Screen orientation (0-3) |
| 3 | Temperature unit (0=°C default / 1=°F) |
| 10-29 | City name (length byte + chars) |
| 30-125 | WiFi ssid/psw struct |
| 140-141 | Health count date key (month×100+day) |
| 142-147 | Today's counters for the six health activities (`HEALTH_ACTS`) |
| 148-149 | Spare (health counter expansion) |
| 150-194 | Device identity deviceId + token (written by `0x06` / erased by `0x07`) |

After a counter-slot expansion, the first read of fresh 0xFF bytes triggers a one-time full counter reset (>9 = invalid) — expected, safe behavior.

### Device credentialing (before shipping, ~2 min each)

1. Flash the same firmware for the whole batch (one .bin); on first boot the splash shows the MAC-derived deviceId (e.g. `TT-A1B2C3`)
2. Generate a token (`crypto.randomBytes(16).toString('hex')`), write it via serial `0x06 <deviceId> <token>`
3. Add the same account in the EMQX Cloud dashboard Authentication (username=deviceId / password=token)
4. Verify the link (send a message via `node dsh-mqtt-live-test.cjs`, or `node send-to-device.cjs thinking` then `done`) → record in ledger.csv (deviceId, token, date) → ship

The deviceId is hardware-anchored: not choosable, not enumerable. Un-credentialed devices skip MQTT entirely; clock and weather run normally and the splash suggests the ID.

### MQTT message protocol

Broker: **EMQX Cloud Serverless** (free tier ≈ 23 devices online 24/7). One identity per device, and **the device and the PC plugin share the same credential pair**:

- **deviceId**: MAC-derived (e.g. `TT-A1B2C3`) — also the MQTT username
- **token**: random secret — the MQTT password, shipped with the device card (pasted into the plugin config)

Topic: `rubato/<deviceId>/state` — each device subscribes only to its own topic; the plugin publishes with the same credential pair. The config exists in two copies kept identical (workspace root `dsh-mqtt-config.json` and `Rubato_Plugin_DSH/dsh-mqtt-config.json`; the plugin looks at the former first), **hot-reloaded per record** — edits apply without restart.

```json
{"model": "glm-5.3-flash", "state": "thinking", "ts": 1788000000000}
{"state": "generating"}
{"state": "Estimate", "estSec": 57.5}
{"state": "done"}
```

`state` accepts `thinking`/`generating` to drive the orb; `done` ends the cycle; `Estimate` only books the estimate (used for the threshold); plain text is ignored. Device-side parsing is **case-insensitive** (internally lowercased before matching) — the plugin's `Thinking`/`Done` are accepted as-is.

The config file **must set `"tls": true` explicitly** — the plugin's built-in default `tls: false` is a leftover from the Aliyun plaintext-1883 era; without the key it connects plaintext to EMQX's 8883 and the server drops it (symptom: `connection closed by server`). The file must be saved **without a BOM** (Node's `JSON.parse` rejects a BOM).

**clientId convention**: `rubato-<source tool>[-p<pid>]` — `rubato-dsh-p3a7` (DSH plugin; the process suffix is auto-appended to avoid collisions), later `rubato-codex` / `rubato-claude` / `rubato-cursor`. The EMQX Cloud Serverless shared frontend **does not accept same-clientId session takeover** (a duplicate is rejected with CONNACK 2), so every source and every process instance needs a unique clientId; one device topic accepts multiple concurrent publishers — a new tool reuses the same host/credentials/topic with its own clientId.

### Full integration (driven by real DSH sessions)

The `rubato` plugin registers in `~/.dsh/profiles/web/cordis.patch.yml`, package at `~/.dsh/profiles/node_modules/rubato/` (host-only, loaded with dsh). It hooks `llm/stream` inside the host process: every model call automatically publishes Estimate (a zero-token kNN duration estimate) → Thinking → Generating → Done (with token-usage backfill as calibration samples), fire-and-forget, never blocking the conversation; intermediate steps of multi-tool calls publish only Thinking, the last step publishes Done.

**Low-latency design**: the plugin keeps **one persistent MQTT connection** to the broker (PINGREQ keepalive; the next publish reconnects automatically after a drop) — each message is a single packet: ~200ms for the first publish including connect, ~35ms after that; Thinking publishes as soon as the stream starts (synchronized with the DSH UI, not waiting for the first reasoning token, so the device doesn't fall behind during large-context prefill); on stream errors a Done is still sent so the device never hangs mid-breath. Concurrent publishes serialize at the connection layer (single leader connects, followers wait and reuse) — eliminating same-clientId races.

For manual verification use `Rubato_Plugin_DSH/send-to-device.cjs`; plugin code changes need a DSH restart (the config is the exception — hot-reloaded).

---

## 6. Tooling

Health icon pipeline (`tools/logos/`):

1. Source: six full-color SVGs (fluent-emoji-flat droplet / toilet / meditation / peach / person-standing + a hand-drawn flat eye) in `tools/icons/color/` (since V1.4.9, full-color emoji art directly; the earlier monochrome outline approach was dropped — `_fetch/_color` are legacy tools, used only when a new activity needs theme-color tinting)
2. `_gen_health_icons.cjs`: renders 100×100 black-background JPEGs and generates `img/health_icons.h` (PROGMEM arrays); the `PICKS` array decides which six are enabled (`ICON_SIZE` stays in sync with the firmware `drawJpg` call)

Run `npm install` inside `tools/logos/` first (depends on sharp). Icon source: Microsoft Fluent Emoji flat (MIT), free for commercial use — the style fits the black-background pages best.

---

## 7. Scope Decisions

| Topic | Decision | Why |
|-------|----------|-----|
| estSec on screen | ❌ no | predictions are unreliable; the screen shows facts only (elapsed time) |
| Token/Cost completion card | ❌ removed | unrelated to the health mission |
| Memory / completion rate | ⏸️ deferred | no status return channel — fake data is worse than none; delivered reminders are enough |
| Health/medical claims | ❌ never | behavioral reminders only; keeps regulators and pseudoscience out |
| Large per-frame heap allocations | ❌ eliminated | heap-fragmentation lesson: 8-bit color canvas + direct-draw fallback; the date never disappears |
| Firmware size budget | **≤ ~1019KB/build** | 4MB flash with the 4M2M layout: two OTA slots of ~1020KB each — a bin over the limit cannot OTA (the compiler's "66%" is single-slot usage, not 4MB total); the 2MB filesystem is unused by code, OTA erases only the target slot, EEPROM settings are unaffected |
| ℃/℉ glyphs | ❌ no font extension | all bundled fonts (number font + FreeFonts) are pure ASCII (verified in library source); the degree symbol is synthetically drawn (digit + hand-drawn dot + letter); a full non-ASCII copy pass would trigger evaluating FS + vlw smooth fonts |
| Mixed-weight copy | ❌ reverted | bold quantity/duration + regular rest looked "off"; all-bold unified (V1.4.9 experiment) |

---

## 8. Roadmap

**Shipped**: V1.0 western-audience overhaul (2026-08-15/16) → V1.1 AI breathing orb + color system (2026-08-25) → V1.2 full-screen health reminder takeover (2026-08-25) → V1.3 MQTT multi-device + official DSH integration (2026-08-30) → V1.4 OTA self-upgrade + core 3.1.2 migration (2026-08-30, full OSS HTTPS chain verified on hardware) → V1.4.9 NTP three-server failover + OTA anti-brick guard + boot splash v2 + 5th health activity Kegels + temperature unit switch (2026-08-30) → release numbering (from V1.0.2) + boot-mode OTA end-to-end closed loop → MQTT certificate pinning (chain + hostname + dual-clock fix) + 6th health activity standing desk (2026-08-31) → renamed to **Rubato** (2026-08-31)

**Planned**:

- [x] OTA (shipped in V1.4; anti-brick guard added in V1.4.9): MQTT notify (`{"state":"ota","ver":...,"url":...,"md5":...}`) → `cmpVersion` gate → **segmented HTTPS download** (one connection per 256KB, HTTP Range + resume) → `setMD5` verify → reboot into the new slot; bin ≤ ~1019KB (see Scope Decisions). Integrity model: md5 delivered over the TLS-encrypted MQTT channel; download channel https (OSS/GitHub Releases) or http (LAN testing). Artifact naming `ota-releases\Rubato-<ver>.bin`; build fixed `eesz=4M2M` (see changelog V1.4). Anti-brick: RTC boot counter + Safe Mode (3 consecutive armed boots without disarming → minimal services + OTA listener only; fixed images self-heal remotely).
- [ ] AI session history is accumulating naturally on the PC side in `rubato-records.jsonl`; Memory personalization deferred, data ready
- [ ] Codex / Claude Code / OpenClaw adapters
- [ ] 24-hour clock and other defaults

---

## Changelog

See [changelog.md](changelog.md) for every update.
