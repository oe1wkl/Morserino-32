# M32 USB Serial Protocol — Command Matrix

Three-corner audit of the USB serial protocol. The three corners ("sources"):

- **DOC** — `Documentation/Protocol Description/M32 Protocol.md` (spec v1.3, dated 2026‑04‑03).
- **FW** — firmware serial layer, all in `Software/src/Version 6 and newer/`:
  `serialEvent()` → `serialDecode()` → `m32Get()`/`m32Put()` (`m32_v6.ino:3691‑4270`),
  JSON output in `MorseJSON.cpp`.
- **UTIL** — host tools in `Software/Utilities/`: `m32_config_tool.html` (live config editor)
  and `m32_file_manager.html` (file upload/manage). Both use WebSerial @ 115200,
  terminator `\n`.

Audit HEAD: `765a596`. Analysis only — no code or doc was changed. Behaviour was read
from **code**, not from the document's claims. Companion files:
[`conflicts.md`](conflicts.md), [`PROTOCOL_SPEC.draft.md`](PROTOCOL_SPEC.draft.md),
[`utility-enhancements.md`](utility-enhancements.md).

---

## Summary — the headline findings

**The good news — the three corners are broadly aligned.** Every command in the document
is implemented in firmware, and every command either tool sends is a real firmware command.
Framing is uniform: host→device is `GET`/`PUT` + space + slash‑separated args, terminated by
a single `\n` (LF); device→host is one brace‑delimited JSON object with **no terminator**.
`GET`/`PUT`/object/specifier are case‑folded to lower‑case; only the trailing *value* keeps
its case. Round‑trip by *parameter name* (not index) makes config edits portable across
firmware builds.

**The biggest problems (full detail + citations in `conflicts.md`):**

1. **`device.firmware` was always empty (HIGH) — ✅ FIXED 2026-06-22.** The global `vsn`
   (`m32_v6.ino:183`) was *never assigned*, so `GET device` / `PUT device/protocol/on` returned
   `"firmware":""`. Now assigned in `setup()` from `VERSION_MAJOR.MINOR[.PATCH]` → reports the real
   version. Protocol stays 1.3 (fix is an implementation detail). See `conflicts.md` C1.
2. **`PUT device/reset/defaults` did nothing on a configured device (HIGH) — ✅ FIXED 2026-06-22.**
   `resetDefaults()` (`MorsePreferences.cpp:1615`) re‑persisted the *current in‑RAM* `pliste[].value`
   rather than restoring compile‑time defaults (boot had already overwritten them, `readPreferences`
   line 1449). Now uses a boot‑surviving flag + reboot so the reset runs early in `setup()` before
   stored values load. Side effect: the command reboots → host must reconnect. See `conflicts.md` C2.
3. **Error object key mismatch (MEDIUM).** DOC says `{"error":{"name":"…"}}`; FW emits
   `{"error":{"content":"…"}}` (`jsonError`→`jsonCreate`, `MorseJSON.cpp:30,147`). Any
   doc‑following client reads the wrong field. (UTIL dodges it — it never parses errors at all.)
4. **`GET control/volume` reports swapped bounds (MEDIUM).** Returns `"minimum":19,"maximum":0`
   — the call passes `volumeMax,volumeMin` in the min/max slots (`m32_v6.ino:3797`). Inert in the
   shipped tool (it hardcodes 0–19 and ignores the response bounds), but wrong for any other client.

**The most important coverage gaps in the "backup" path (full detail in `utility-enhancements.md`):**

- **There is no file‑based backup/restore at all.** The "config tool" is a *live editor*: it
  has no export‑to‑file and no import‑from‑file. You cannot capture a device's full
  configuration to disk or push one back.
- **Game state is unreachable.** Invaders (`m32game`), Morsel (`morsel`), and Radio Cave
  (`radiocave`) scores/saves have **no protocol command** — they can be neither read nor written.
- **Asymmetric settings break round‑trip.** WiFi passwords are **write‑only** (never read back,
  `jsonGetWifi`, `MorseJSON.cpp:303`); hardware calibration (brightness, leftHanded, vAdjust,
  LoRa band/QRG/power) is **read‑only** via `GET hardware` and cannot be written.
- **No protocol versioning handshake.** `M32P_VERSION` is only echoed inside the device object
  (whose `firmware` field is broken, see #1); there is no `GET`/negotiation a tool can rely on
  to adapt across firmware versions.

---

## Conventions used in the table

- **Dir**: H→D (host→device command), D→H (device→host response/async), R/R (request→response pair).
- **Term (in)**: terminator the *host* must send — uniformly `\n` (LF) for every command.
- **Resp**: the device→host reply (always a JSON object, no terminator), or "—" for none.
- Presence: ✓ = present, ✗ = absent, ⚠ = present but diverges (see `conflicts.md`).

---

## Session / device commands

| ID | Token (canonical) | Dir | DOC | FW | UTIL | Resp on success | Notes |
|---|---|---|---|---|---|---|---|
| `device.info` | `GET device` | R/R | ✓ §Device Info | ✓ `m32Get` 3803 | ~ (via on) | `{"device":{hardware,firmware,protocol,build}}` | `firmware` populated (✅ C1); `build` = compile date (additive, 1.3) |
| `device.protocol.on` | `PUT device/protocol/on` | R/R | ✓ | ✓ `m32Put` 3905; pre‑gate `serialEvent` 3711 | ✓ connect | `{"device":{…}}` | only command honoured while protocol OFF |
| `device.protocol.off` | `PUT device/protocol/off` | R/R | ✓ | ✓ 3906 | ✓ disconnect | `{"end m32protocol":{"content":"Goodbye!"}}` | response undocumented (C9) |
| `device.reset.defaults` | `PUT device/reset/defaults` | R/R | ✓ §Device Info | ✓ flag+reboot 3915 → `setup()`/`resetDefaults` 1615 | ✓ `resetDefaults()` 796 | `{"ok":{"content":"OK"}}` then reboot | **now reboots to apply (✅ C2)** |

## Control (speed / volume)

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `control.get` | `GET control/{speed\|volume}` | R/R | ✓ | ✓ 3793 | ✓ dashboard 813,822 | `{"control":{name,value,minimum,maximum}}` | **volume min/max swapped (⚠ C4)** |
| `controls.get` | `GET controls` | R/R | ✗ | ✓ 3801 → `jsonControls` | ✗ | `{"controls":[{name,value}×2]}` | undocumented (C8) |
| `control.put.speed` | `PUT control/speed/<wpm>` | R/R | ✓ | ✓ 3894 → `changeSpeed` | ✓ 862 (fire‑and‑forget) | `{"control":{name:"speed",value}}` | not `ok` — control echo |
| `control.put.volume` | `PUT control/volume/<0‑19>` | R/R | ✓ | ✓ 3897 → `changeVolume` | ✓ 870 (fire‑and‑forget) | `{"control":{name:"volume",value}}` | abs‑set via relative math, correct |

## Menu

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `menus.get` | `GET menus` | R/R | ✓ | ✓ 3809 → `jsonMenuList` 49 | ✓ 840 | `{"menus":[{content,menu number,executable}]}` | numbering 1..menuN‑1; menuN is variant‑dependent |
| `menu.get` | `GET menu` | R/R | ✓ | ✓ 3805 | ✓ 830 | `{"menu":{content,menu number,executable,active}}` | `active=false` only when `m32state==menu_loop` |
| `menu.set` | `PUT menu/set/<n>` | R/R | ✓ | ✓ 4185 | ✗ | `{"ok":…}` | selects (even non‑executable) |
| `menu.start` | `PUT menu/start[/<n>]` | R/R | ✓ | ✓ 4159 | ✓ 880 | `{"ok":…}` | requires `m32state==menu_loop` |
| `menu.startnow` | `PUT menu/start now[/<n>]` | R/R | ✓ | ✓ 4159 (`token=="start now"`) | ✓ 885 | `{"ok":…}` | skips paddle‑arm; note literal space in token |
| `menu.stop` | `PUT menu/stop` | R/R | ✓ | ✓ 4155 | ✓ 875 | `{"ok":…}` | returns to main menu |

## Configuration (parameters)

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `configs.get` | `GET configs` | R/R | ✓ | ✓ 3813 → `jsonParameterList` 80 | ✓ 899 | `{"configs":[{name,value,displayed}]}` | iterates `i<=posSerialOut`; **set is variant‑dependent (C7)** |
| `config.get` | `GET config/<name>` | R/R | ✓ | ✓ 3811 → `jsonParameter` 61 | ✓ 907 | `{"config":{name,value,description,minimum,maximum,step,isMapped,mapped values}}` | name match case‑insensitive |
| `config.put` | `PUT config/<name>/<numvalue>` | R/R | ✓ | ✓ 3970 → `setParameter` 4273 | ✓ 1025 | `{"ok":…}` / `{"error":…}` | numeric value only; by‑name round trip |

## Snapshots

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `snapshots.get` | `GET snapshots` | R/R | ✓ | ✓ 3815 → `jsonSnapshots` 194 | ✓ 1051 | `{"snapshots":{"existing snapshots":[…]}}` | 1‑based numbers |
| `snapshot.get` | `GET snapshot/<n>` | R/R | ✓ (v1.3) | ✓ 3817 → `jsonGetSnapshot` 363 | ✓ 1076 | `{"snapshot":{number,lastExecuted,menuName,customChars,configs[]}}` | reads `snapN` NVS namespace; non‑destructive; no `PUT` counterpart |
| `snapshot.store` | `PUT snapshot/store/<n>` | R/R | ✓ | ✓ 3978 | ✓ 1068 | `{"ok":…}` | n=1..8; captures current state |
| `snapshot.recall` | `PUT snapshot/recall/<n>` | R/R | ✓ | ✓ 3989 | ✓ 1069 | `{"ok":…}` | applies snapshot |
| `snapshot.clear` | `PUT snapshot/clear/<n>` | R/R | ✓ | ✓ 3989 | ✓ 1070 | `{"ok":…}` | deletes `snapN` |

## Player‑text file (file player)

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `file.size` | `GET file/size` | R/R | ✓ | ✓ 3838 → `jsonFileStats` 209 | ✗ | `{"file":{size,free}}` | SPIFFS stats |
| `file.text` | `GET file/text` | R/R | ✓ | ✓ 3840 → `jsonFileText` 240 | ✓ 1302 | `{"file":{text}}` | strips `{` `}`, escapes control chars |
| `file.firstline` | `GET file/first line`, `GET file` | R/R | ✓ | ✓ 3842 → `jsonFileFirstLine` 223 | ✗ | `{"file":{"first line":…}}` | strips `{` `}` |
| `file.new` | `PUT file/new/<text>` | R/R | ✓ | ✓ 4013 | ✗ (uses begin/data/end) | `{"ok":…}` | overwrites player.txt |
| `file.append` | `PUT file/append/<text>` | R/R | ✓ | ✓ 4023 | ✗ | `{"ok":…}` | append a line |
| `file.parts` | `GET file/parts` | R/R | ✓ | ✓ 3846 | ✗ | `{"fileparts":{count,selected,parts[]}}` or `{"fileparts":{content:"Single file…"}}` | multipart text |
| `file.part` | `PUT file/part/<n>` | R/R | ✓ | ✓ 4108 | ✗ | `{"filepart":{name,selected,count}}` | select multipart part |

## Arbitrary / binary file transfer

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `file.begin` | `PUT file/begin/<filename>` | R/R | ✓ (v1.2) | ✓ 4030 | ✓ 1363,1610 / fm 402 | `{"ok":…}` | opens upload; auto‑prefixes `/` |
| `file.data` | `PUT file/data/<base64>` | R/R | ✓ | ✓ 4048 | ✓ 1369,1616 / fm 410 | `{"ok":…}` / `{"error":…}` | **decode buffer 256 B; ~200 B/chunk (C6)** |
| `file.end` | `PUT file/end` | R/R | ✓ | ✓ 4072 | ✓ 1374,1621 / fm 416 | `{"upload":{file,size}}` | closes, rescans parts |
| `file.list` | `GET file/list` | R/R | ✓ | ✓ 3844 → `jsonFileList` 273 | ✓ 1274 / fm 344 | `{"files":[{name,size}],total,used,free}` | full SPIFFS listing |
| `file.delete` | `PUT file/delete/<filename>` | R/R | ✓ | ✓ 4096 | ✓ 1293 / fm 363 | `{"ok":…}` / `{"error":…}` | |

## WiFi configuration

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `wifi.get` | `GET wifi` | R/R | ✓ | ✓ 3862 → `jsonGetWifi` 299 | ✓ 1163 | `{"wifi":[{ssid,trxpeer,selected}×3],espnow,wlanChoice}` | **password never returned (write‑only, C5)** |
| `wifi.ssid` | `PUT wifi/ssid/<n>/<ssid>` | R/R | ✓ | ✓ 4144 → `setSsid` 4319 | ✓ 1190 | `{"ok":…}` | n=1..3 |
| `wifi.password` | `PUT wifi/password/<n>/<pw>` | R/R | ✓ | ✓ 4146 → `setPassword` 4339 | ✓ 1191 | `{"ok":…}` | write‑only |
| `wifi.trxpeer` | `PUT wifi/trxpeer/<n>/<peer>` | R/R | ✓ | ✓ 4148 → `setTrxPeer` 4360 | ✓ 1192 | `{"ok":…}` | n=1..3 |
| `wifi.select` | `PUT wifi/select/<n>` | R/R | ✓ | ✓ 4142 → `selectWifi` 4380 | ✓ 1193 | `{"ok":…}` | **n=0 selects EspNow — undocumented (C10)** |

## Koch lesson / custom characters

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `kochlesson.get` | `GET kochlesson` | R/R | ✓ | ✓ 3865 → `jsonGetKoch` 97 | ✓ 1202 | `{"kochlesson":{value,minimum,maximum,characters[]}}` | |
| `kochlesson.put` | `PUT kochlesson/<n>` | R/R | ✓ | ✓ 4200 | ✓ 1231 | `{"ok":…}` | range = koch min..max |
| `customchars.get` | `GET customchars` | R/R | ✓ (v1.3) | ✓ 3828 → `jsonGetCustomChars` 426 | ✓ 1235 | `{"customchars":{active,characters}}` | |
| `customchars.set` | `PUT customchars/set/<chars>` | R/R | ✓ | ✓ 3946 | ✓ 1251 | `{"ok":…}` | **value case‑sensitive**; empty ⇒ clear |
| `customchars.clear` | `PUT customchars/clear` | R/R | ✓ | ✓ 3959 | ✓ 1260 | `{"ok":…}` | |

## Player identity

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `player.get` | `GET player` | R/R | ✓ (v1.3) | ✓ 3825 → `jsonGetPlayer` 412 | ✓ 1390 | `{"player":{call,name}}` | reads `morserino` NVS |
| `player.call` | `PUT player/call/<call>` | R/R | ✓ | ✓ 3923 | ✓ 1399 | `{"ok":…}` | upper‑cased, truncated to 8 |
| `player.name` | `PUT player/name/<name>` | R/R | ✓ | ✓ 3932 | ✓ 1400 | `{"ok":…}` | upper‑cased, truncated to 8 |

## Hardware / battery (read‑only)

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `hardware.get` | `GET hardware` | R/R | ✓ (v1.3) | ✓ 3831 → `jsonGetHardware` 434 | ✓ 763 | `{"hardware":{brightness,leftHanded,vAdjust[,loraBand,loraFrequency,loraPower]}}` | LoRa fields gated `#ifndef LORA_DISABLED`; **read‑only — no writer** |
| `battery.get` | `GET battery` | R/R | ✓ (v1.3) | ✓ 3834 → `jsonGetBattery` 448 | ✓ 778 | `{"battery":{[voltage,]status}}` | voltage gated `#ifdef CONFIG_MCP73871` |

## Automated CW keyer & memory keyer

| ID | Token | Dir | DOC | FW | UTIL | Resp | Notes |
|---|---|---|---|---|---|---|---|
| `cw.play` | `PUT cw/play/<text>` | R/R | ✓ | ✓ 4211 | ✗ | `{"ok":…}` / `{"error":…}` | only in `morseKeyer`/`morseTrx` |
| `cw.repeat` | `PUT cw/repeat/<text>` | R/R | ✓ | ✓ 4211 | ✗ | `{"ok":…}` | repeats until stop |
| `cw.stop` | `PUT cw/stop` | R/R | ✓ | ✓ 4238 → `stopPlayCw` 4400 | ✗ | `{"ok":…}` | |
| `cw.store` | `PUT cw/store/<n>/<content>` | R/R | ✓ | ✓ 4241 | ✓ 1152,1154 | `{"ok":…}` | n=1..8; empty ⇒ delete; truncated 47 chars |
| `cw.recall` | `PUT cw/recall/<n>` | R/R | ✓ | ✓ 4211 | ✓ 1153 | `{"ok":…}` | n<3 ⇒ repeat |
| `cw.memories` | `GET cw/memories` | R/R | ✓ | ✓ 3869 → `jsonGetCwStores` 326 | ✓ 1126 | `{"CW Memories":{"cw memories in use":[…]}}` | |
| `cw.memory` | `GET cw/memory/<n>` | R/R | ✓ | ✓ 3872 → `jsonGetCwStore` 342 | ✓ 1134 | `{"CW Memory":{number,content}}` | error if empty |

## Asynchronous device→host messages (not commands)

Emitted spontaneously on user activity at the device (when protocol is ON), and also as
`GET` responses. Documented in DOC §"JSON objects sent by Morserino"; emitted throughout
`MorseMenu.cpp`, `MorsePreferences.cpp`, `m32_v6.ino`.

| Object | Shape | DOC | FW | Notes |
|---|---|---|---|---|
| `menu` | `{content,menu number,executable,active}` | ✓ | ✓ `jsonMenu` 38 | on menu navigation |
| `control` | `{name,value}` | ✓ | ✓ `jsonControl` 166 (`detailed=false`) | on live speed/volume change |
| `config` | `{name,value,displayed}` | ✓ | ✓ `jsonConfigShort` 138 | on parameter‑menu navigation |
| `activate` | `{state}` ∈ EXIT/ON/SET/CANCELLED/RECALLED/CLEARED | ✓ | ✓ `jsonActivate` 156 | on mode activate/exit |
| `message` | `{content}` | ✓ | ✓ `jsonCreate("message",…)` 147 | transient on‑screen messages |
| `ok` | `{content:"OK"}` | ✓ | ✓ `jsonOK` 34 | success ack |
| `error` | DOC `{name}` / FW `{content}` | ⚠ | ⚠ `jsonError` 30 | **key mismatch (C3)** |

Plus the raw **keyed/decoded/generated character stream**, which is *not* JSON and is gated by
the `Serial Output` parameter (independent of `m32protocol`). DOC §"Receiving information"
acknowledges it; it shares the same USB port but no framing.

---

## Variant note (axis 23)

Both hardware variants compile the **same** serial code (`m32_v6.ino`, `MorseJSON.cpp`) — there is
no variant‑specific parser. Divergence is entirely **data‑driven by compile‑time flags**:

- `GET hardware` LoRa fields appear only `#ifndef LORA_DISABLED` (Classic yes, Pocket no).
- `GET battery` voltage appears only `#ifdef CONFIG_MCP73871` (Pocket yes, Classic "usb powered").
- `GET configs` includes `Headphone Output` only `#ifdef CONFIG_SOUND_I2S` (Pocket only) — so the
  parameter set, and therefore valid `config/<name>` targets, **differ between variants**. The tool
  hardcodes `Headphone Output` in its General group (`m32_config_tool.html:577`); on Classic that
  target returns `INVALID PARAMETER`.
