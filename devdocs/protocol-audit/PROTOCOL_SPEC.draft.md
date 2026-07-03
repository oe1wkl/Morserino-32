# M32 USB Serial Protocol — Normative Specification (DRAFT)

> **Status: DRAFT — decisions ratified 2026-06-22.** This is the protocol analogue of
> `devdocs/UX_CONVENTIONS.md`; once finalised it becomes the single source of truth for the USB
> serial protocol, and the existing `M32 Protocol.md` is updated to match (the 1.3 doc updates have
> landed). All `PENDING-DECISION` items below have been resolved or deferred to protocol 1.4 — see the
> **Resolution status** table in [`conflicts.md`](conflicts.md) and [`RESOLUTION_PLAN.md`](RESOLUTION_PLAN.md);
> The inline `PENDING-DECISION` markers below are kept for traceability but are all resolved or deferred per that table (C3→`content`; C4 fixed; C5/C7/C13 documented; C6 and `GET capabilities` → 1.4).
>
> Every statement here was derived from the **firmware code** at audit HEAD `765a596`
> (`Software/src/Version 6 and newer/`), cross‑checked against the document and the two host tools
> in `Software/Utilities/`.

---

## 1. Transport‑level conventions

These apply to **every** command unless an entry says otherwise.

| Aspect | Specification | Source |
|---|---|---|
| Physical | USB‑CDC serial; additionally (builds with `CONFIG_BLE_SERIAL`, preference "BLE Serial" on) a **BLE Nordic‑UART‑Service transport** carrying the identical byte stream: service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`, RX write char `…0002`, TX notify char `…0003`, name `Morserino-32` in the scan response, no bonding, one central. Notifications chunk at the negotiated MTU (arbitrary split points); per‑transport handshake and `protocol/off` (case‑insensitive whole‑line match on BLE). | `MorseBleSerial.cpp`, `M32 Protocol.md` "Transports" |
| Baud | **115200**, 8‑N‑1, no flow control (USB transport). | `m32_config_tool.html:680`, `m32_file_manager.html:287` |
| Host→device framing | ASCII text line, terminated by a **single `\n` (LF, ASCII 10)**. A trailing `\r` is tolerated (the line is `trim()`‑med). | `m32_v6.ino:3699,3705,3738` |
| Device→host framing | Exactly **one JSON object** per reply (`{ … }`), serialized with ArduinoJson. **No terminator, no leading/trailing whitespace, no newline.** Clients must frame by balanced braces or a quiet‑time timeout. | `MorseJSON.cpp` (all `serializeJson(doc, Serial)`) |
| Encoding | Command + args are ASCII. Payload values are UTF‑8 text; binary payloads use **base64** (`file/data` only). | — |
| Command grammar | `VERB SP object[/specifier[/value]]` where `VERB` ∈ `GET`,`PUT`. `VERB` is separated from the rest by the **first space**; `object`/`specifier`/`value` are separated by `/`. The `value` field keeps everything after the 2nd `/` (so it may itself contain `/` and spaces). | `m32_v6.ino:3739‑3767` |
| Case | `VERB`, `object`, `specifier` are folded to lower‑case before dispatch. **`value` keeps its case.** Parameter‑name matching is therefore case‑insensitive; `customchars/set`, `cw/*`, `wifi/*`, `player/*`, `file/*` values are case‑sensitive. | `m32_v6.ino:3743‑3765`, `4273‑4281` |
| Enable gate | While the protocol is **OFF**, the only command honoured is `put device/protocol/on` (case‑folded compare); all other input is ignored. The raw character stream (keyed/decoded/generated chars) is independent and gated by the `Serial Output` parameter, not by the protocol. | `m32_v6.ino:3707‑3715` |
| Sleep | While the protocol is ON, the device time‑out (auto‑sleep) is suspended. | DOC §"Enabling…"; behaviour set on `protocol/on` |
| Timeouts / pacing | No device‑side ACK timing is specified. Hosts poll for the reply; the shipped tools use 2–5 s per‑command timeouts and ~100 ms inter‑command spacing, and pace large transfers one `file/data` chunk at a time. | `m32_config_tool.html:654‑672, 1363‑1374` |
| Max payload | A single `file/data` chunk must decode to **≤256 bytes** (oversize ⇒ error, not overflow). No explicit cap on other command lengths — see C15. | `m32_v6.ino:4060`, C14/C15 |

### 1.1 Response model

- **Success (most `PUT`)** → `{"ok":{"content":"OK"}}` (`jsonOK`, `MorseJSON.cpp:34`).
- **Success (`PUT control/*`)** → a **`control`** object echoing the new value (not `ok`):
  `{"control":{"name":"speed"|"volume","value":N}}` (`changeSpeed`/`changeVolume`,
  `m32_v6.ino:2598,2617`). `minimum`/`maximum` are omitted here.
- **Success (`GET`)** → the object described per command (§4).
- **Error** → `{"error":{"content":"<message>"}}` (`jsonError`→`jsonCreate`, `MorseJSON.cpp:30,147`).
  **RESOLVED (C3, 2026-06-22):** `content` is canonical; the document was corrected to match the firmware.

### 1.2 Error conditions emitted by the parser (before a command‑specific handler runs)

| Condition | Message (`content`) | Source |
|---|---|---|
| No argument after the verb | `MISSING ARGUMENT` | `m32_v6.ino:3772` |
| Verb not `get`/`put` | `COMMAND NOT RECOGNIZED` | `m32_v6.ino:3782` |
| `GET` with empty object | `ARGUMENT(S) MISSING` | `m32_v6.ino:3790` |
| `PUT` with empty object/specifier | `NOT ENOUGH ARGUMENTS` | `m32_v6.ino:3888` |
| Unknown `GET` object | `GET <obj>: UNKNOWN COMMAND` | `m32_v6.ino:3880` |
| Unknown `PUT` object | `COMMAND <obj> NOT YET IMPLEMENTED` | `m32_v6.ino:4269` |

> **Consistency note**: error *mechanism* is uniform (always an `error` object), but the message
> *strings* are ad‑hoc and not enumerable by a client. The spec should treat the `content` text as
> human‑readable only, never machine‑parsed.

---

## 2. Session lifecycle

1. Host opens the port at 115200 and sends `put device/protocol/on`.
2. Device replies with the **device object** and suspends auto‑sleep.
3. Host issues `GET`/`PUT` commands; device replies one object each.
4. Host sends `put device/protocol/off` to end; device replies
   `{"end m32protocol":{"content":"Goodbye!"}}` and re‑enables auto‑sleep. `PENDING-DECISION (C12)`:
   bless or rename this farewell object.

Device object (reply to `device/protocol/on` and `GET device`):
```json
{"device":{"hardware":"<string>","firmware":"<string>","protocol":"1.3","build":"<compile date>"}}
```
- `hardware` — `PENDING-DECISION (C9)`: real values are `"M32 1st edition"`, `"M32 2nd edition"`,
  `"unknown M32 board"`, the board macro `HW_NAME` (e.g. the Pocket), or `"Unknown Device"` — not the
  DOC's `"1st edition"`/`"2nd edition"`.
- `firmware` — the firmware version, e.g. `"8.1"` (`vsn`). **RESOLVED (C1, 2026-06-22):** was
  previously always `""` because `vsn` was never assigned; now set in `setup()`.
- `protocol` — `M32P_VERSION`, currently `"1.3"` (`morsedefs.h`). (The reset-reboot, the
  `firmware`-field fix, and the `build` field below all landed as additive 1.3 changes — no version bump.)
- `build` — the firmware compile date (`COMPILEDATE` = `__DATE__`), e.g. `"Jun 22 2026"` (added
  2026-06-22 as an additive build stamp; clients that don't know it simply ignore the key).

---

## 3. Versioning (axis 16)

`PENDING-DECISION (C‑VER)`. Current reality:

- The only version signal is `device.protocol` (and the broken `device.firmware`). There is **no
  negotiation command** and **no capability list**; a host cannot ask "do you support `GET battery`?"
  except by trying it and catching `UNKNOWN COMMAND`.
- The protocol version is a flat string (`"1.3"`); there is no documented compatibility rule
  (is 1.3 a superset of 1.2? — in practice yes, additive).

Recommended spec content once ratified (see `utility-enhancements.md` for the host side):
1. Fix `device.firmware` (C1) so version detection is possible at all.
2. State the compatibility contract: protocol versions are **additive within a major**; a client
   should treat unknown objects/keys as ignorable and unsupported commands as absent.
3. Consider a lightweight `GET capabilities` (list of supported objects) for forward‑compat.

---

## 4. Command reference

Template per command: **Token · Direction · Params · Response · Errors · Semantics / side effects.**
All inherit the §1 framing. Citations: FW = `m32_v6.ino`/`MorseJSON.cpp`; DOC section; UTIL function.

### 4.1 Device

**`GET device`** · D→H · no params · → device object (§2) · Semantics: read hardware/firmware/protocol.
FW `m32Get` 3803. `PENDING-DECISION (C1,C9,C16)`.

**`PUT device/protocol/{on|off}`** · R/R · → device object (on) / farewell (off) · Semantics: enable
/disable the protocol; `on` also suspends sleep, `off` restores it. Sending `on` while already on
re‑sends the device object. FW 3905‑3913.

**`PUT device/reset/defaults`** · R/R · → `ok`, **then the device reboots** · Restores all
`pliste` parameters to factory defaults, preserving snapshots / WiFi / CW memories / player identity.
**RESOLVED (C2, 2026-06-22):** the handler sets NVS `morserino/factoryReset=1`, acknowledges, and
`ESP.restart()`s; `setup()` clears the flag and runs `resetDefaults()` before `readPreferences()`
(where `pliste[]` still holds compile-time defaults). The reboot switches the protocol OFF and drops
the connection — **the host must reconnect** and re-send `device/protocol/on`. FW 3915 → `setup()` ~705
→ `MorsePreferences.cpp:1615`.

### 4.2 Control (speed / volume)

**`GET control/{speed|volume}`** · R/R · → `{"control":{name,value,minimum,maximum}}`. Speed bounds
`5..60` (`wpmMin/Max`); volume bounds `0..19` (`volumeMin/Max`). **RESOLVED (C4, 2026-06-22):** the
volume reply previously reported `minimum:19,maximum:0` (swapped args); fixed in firmware. FW 3793‑3797.

**`GET controls`** · R/R · → `{"controls":[{name:"speed",value},{name:"volume",value}]}` ·
**RESOLVED (C10):** kept and now documented. FW 3801, `jsonControls` 179.

**`PUT control/speed/<wpm>`** · R/R · value = absolute words‑per‑minute, clamped 5..60 · →
`{"control":{name:"speed",value}}` · Side effect: updates live keyer speed + timings. (Internally
converted to a relative delta; absolute target is honoured.) FW 3894 → `changeSpeed`.

**`PUT control/volume/<0‑19>`** · R/R · value = absolute volume, clamped 0..19 · →
`{"control":{name:"volume",value}}` · Side effect: live sidetone volume (+ codec on Pocket). FW 3897
→ `changeVolume`.

### 4.3 Menu

**`GET menus`** · R/R · → `{"menus":[{content,menu number,executable}]}`, numbers `1..menuN‑1`.
The set is **variant‑dependent** (LoRa/Games/QSO‑Bot flags change `menuN`). FW 3809, `jsonMenuList` 49.

**`GET menu`** · R/R · → `{"menu":{content,menu number,executable,active}}`; `active=false` iff
`m32state==menu_loop` (i.e. true while a mode runs). FW 3805.

**`PUT menu/set/<n>`** · R/R · → `ok`/`error` · select menu entry n (even if not executable); does not
start it. Errors `INVALID argument <n>` if out of range. FW 4185.

**`PUT menu/start[/<n>]`** · R/R · → `ok`/`error` · with no value: start the currently selected entry
if executable and in the menu loop; with `<n>`: select+start entry n. Errors
`NOT EXECUTABLE - Menu No <n>`. FW 4159.

**`PUT menu/start now[/<n>]`** · R/R · as `start`, but CW‑Generator modes begin immediately instead of
waiting for a paddle/key. Note the literal space in the token `start now`. FW 4159,4164,4176.

**`PUT menu/stop`** · R/R · → `ok` · stop the active mode / leave preferences → main menu. FW 4155.

### 4.4 Configuration (parameters)

**`GET configs`** · R/R · → `{"configs":[{name,value,displayed}]}` for `i=0..posSerialOut`. `value` is
the numeric stored value; `displayed` is the mapped text (or `"Unlimited"` for `Max # of Words`=0).
The set is variant‑dependent — `PENDING-DECISION (C13)`. FW 3813, `jsonParameterList` 80.

**`GET config/<name>`** · R/R · name case‑insensitive · →
`{"config":{name,value,description,minimum,maximum,step,isMapped,mapped values?}}` · `mapped values`
present only when `isMapped`. Error `INVALID PARAMETER`. FW 3811, `jsonParameter` 61.

**`PUT config/<name>/<numericValue>`** · R/R · value is the **numeric** value (never the displayed
text), range‑checked against the parameter's min/max · → `ok` / `error "ERROR setting parameter <name>"`
· Side effects: persists to `morserino`; some params trigger follow‑on logic (`Koch Sequence`,
`Carousel`, mutual exclusion of `wordDoubler`/`AutoStop`). FW 3970 → `setParameter` 4273.

### 4.5 Snapshots

**`GET snapshots`** · R/R · → `{"snapshots":{"existing snapshots":[…]}}` (1‑based). FW 3815.

**`GET snapshot/<n>`** · R/R · n=1..8, must exist · → `{"snapshot":{number,lastExecuted,menuName,
customChars:{active,characters},configs:[{name,value,displayed}]}}`; non‑destructive read from NVS
namespace `snapN`. `configs[]` excludes `Serial Output` and `Time‑out` — `PENDING-DECISION (C17)`.
Error `NO SUCH SNAPSHOT`. FW 3817, `jsonGetSnapshot` 363.

**`PUT snapshot/store/<n>`** · R/R · n=1..8 · → `ok`/`INVALID SNAPSHOT NUMBER` · capture current
parameters + selected menu into `snapN`. FW 3978.

**`PUT snapshot/recall/<n>`** · R/R · n=1..8, must exist · → `ok`/`NO SUCH SNAPSHOT` · apply snapshot to
live settings. FW 3989.

**`PUT snapshot/clear/<n>`** · R/R · n=1..8, must exist · → `ok`/`NO SUCH SNAPSHOT` · delete `snapN`.
FW 3989.

> There is **no** command to push arbitrary snapshot *contents* from the host; snapshots can only be
> captured from current device state, recalled, or cleared. Relevant to backup (axis 19).

### 4.6 File player (player.txt) & arbitrary files

**`GET file/size`** · → `{"file":{size,free}}` (SPIFFS). FW 3838.
**`GET file/text`** · → `{"file":{text}}`; `{`/`}` stripped, control chars escaped. FW 3840.
**`GET file/first line`** / **`GET file`** · → `{"file":{"first line":…}}`; `{`/`}` stripped. FW 3842.
**`PUT file/new/<text>`** · → `ok` · overwrite player.txt with one line; resets word pointer. FW 4013.
**`PUT file/append/<text>`** · → `ok` · append one line. FW 4023.
**`GET file/parts`** · → `{"fileparts":{count,selected,parts[]}}` or a "single file" note. FW 3846.
**`PUT file/part/<n>`** · → `{"filepart":{name,selected,count}}` or error · select multipart part n
(1‑based). FW 4108.

**`PUT file/begin/<filename>`** · → `ok`/`error` · open `filename` (auto‑prefix `/`) for chunked write;
closes any prior incomplete upload. FW 4030.
**`PUT file/data/<base64>`** · → `ok`/`error "Base64 decode error"` / `"No upload in progress"` · decode
and append; **≤256 decoded bytes per chunk** (C14). FW 4048.
**`PUT file/end`** · → `{"upload":{file,size}}` / `"No upload in progress"` · close, rescan multipart,
report size. FW 4072.
**`GET file/list`** · → `{"files":[{name,size}],total,used,free}`. FW 3844, `jsonFileList` 273.
**`PUT file/delete/<filename>`** · → `ok`/`"File not found"`. FW 4096.

### 4.7 WiFi

**`GET wifi`** · → `{"wifi":[{ssid,trxpeer,selected}×3],"espnow":<bool>,"wlanChoice":<n>}`; **password is
never returned** (write‑only, C5). FW 3862, `jsonGetWifi` 299.
**`PUT wifi/ssid/<n>/<ssid>`** (n=1..3) · → `ok`. FW 4144.
**`PUT wifi/password/<n>/<pw>`** (n=1..3) · → `ok`; write‑only. FW 4146.
**`PUT wifi/trxpeer/<n>/<peer>`** (n=1..3) · → `ok`. FW 4148.
**`PUT wifi/select/<n>`** · → `ok` · select entry n=1..3, or **n=0 ⇒ EspNow** — `PENDING-DECISION (C11)`
(undocumented). FW 4142 → `selectWifi` 4380.

### 4.8 Koch & custom characters

**`GET kochlesson`** · → `{"kochlesson":{value,minimum,maximum,characters[]}}`. FW 3865.
**`PUT kochlesson/<n>`** · → `ok`/`INVALID KOCH LESSON` · n in koch min..max. FW 4200.
**`GET customchars`** · → `{"customchars":{active,characters}}`. FW 3828, `jsonGetCustomChars` 426.
**`PUT customchars/set/<chars>`** · → `ok` · **case‑sensitive** value; empty ⇒ disable+clear; rebuilds
the Koch set. FW 3946.
**`PUT customchars/clear`** · → `ok` · disable + clear. FW 3959.

### 4.9 Player identity

**`GET player`** · → `{"player":{call,name}}` (from `morserino` NVS). FW 3825, `jsonGetPlayer` 412.
**`PUT player/call/<call>`** · → `ok` · upper‑cased, truncated to 8 chars. FW 3923.
**`PUT player/name/<name>`** · → `ok` · upper‑cased, truncated to 8 chars. FW 3932.

### 4.10 Hardware / battery (read‑only)

**`GET hardware`** · → `{"hardware":{brightness,leftHanded,vAdjust[,loraBand,loraFrequency,loraPower]}}`;
LoRa fields only `#ifndef LORA_DISABLED`. **No writer exists** — C7. FW 3831, `jsonGetHardware` 434.
**`GET battery`** · → `{"battery":{[voltage,]status}}`; `voltage` + `status` ∈
`charging`/`full`/`no battery` only `#ifdef CONFIG_MCP73871`, else `status:"usb powered"`. `status`
is derived from the MCP73871 charge-controller pins (`MorseOutput::getPowerpathState()`), **not** a
voltage threshold — over USB the cell sits near 4.2 V throughout charging, so voltage can't
distinguish charging from full (fixed 2026-06-22; was previously `v>4100?"full":"charging"`). FW 3834,
`jsonGetBattery` 448.

### 4.11 Automated CW keyer & memory keyer

**`PUT cw/play/<text>`** · → `ok` / `"CW/PLAY: Keyer not active"` · play once; **only in `morseKeyer`
or `morseTrx`** (never LoRa/WiFi Trx — see CLAUDE.md §3 rule 8). `\p`/`<p>` = pause. FW 4211.
**`PUT cw/repeat/<text>`** · → `ok` · as `play`, repeating until stopped. FW 4211.
**`PUT cw/stop`** · → `ok` · stop playback (also stops on manual keying). FW 4238 → `stopPlayCw` 4400.
**`PUT cw/store/<n>/<content>`** · → `ok`/`INVALID CW MEMORY *` · store in memory n=1..8 (empty ⇒
delete); content truncated to 47 chars; prosigns and `[p]`/`\p` allowed. FW 4241.
**`PUT cw/recall/<n>`** · → `ok` · play memory n; n=1 or 2 repeat until stopped. FW 4211.
**`GET cw/memories`** · → `{"CW Memories":{"cw memories in use":[…]}}`. FW 3869, 326.
**`GET cw/memory/<n>`** · → `{"CW Memory":{number,content}}` / `"… IS EMPTY"` / `"INVALID …"`. FW 3872, 342.

---

## 5. Asynchronous device→host messages

When the protocol is ON, user activity at the device emits the same objects used as `GET` responses,
unsolicited (for accessibility / mirroring):

| Object | Shape | Trigger | FW |
|---|---|---|---|
| `menu` | `{content,menu number,executable,active}` | menu navigation | `jsonMenu` 38 |
| `control` | `{name,value}` | live speed/volume change | `jsonControl` 166 |
| `config` | `{name,value,displayed}` | parameter‑menu navigation | `jsonConfigShort` 138 |
| `activate` | `{state}` ∈ EXIT/ON/SET/CANCELLED/RECALLED/CLEARED | mode activate/exit | `jsonActivate` 156 |
| `message` | `{content}` | transient on‑screen text | `jsonCreate("message",…)` 147 |

A client must therefore tolerate **unsolicited objects interleaved** with command replies (the shipped
tools clear `readBuffer` immediately before each command to avoid mixing — `m32_config_tool.html:669`).

Separately, the raw **keyed/decoded/generated character stream** (non‑JSON) is emitted on the same port
per the `Serial Output` parameter, independent of the protocol state. A robust client must be prepared
to see non‑JSON bytes when `Serial Output ≠ none`.

---

## 6. Decisions (index) — ratified 2026-06-22

| Ref | Outcome |
|---|---|
| C1 | ✅ RESOLVED — `device.firmware` populated (`vsn` assigned in `setup()`) |
| C2 | ✅ RESOLVED — `reset/defaults` does a real factory reset via flag+reboot |
| C3 | ✅ RESOLVED — error key is `content`; document corrected |
| C4 | ✅ RESOLVED — swapped volume min/max fixed in firmware |
| C5 | ✅ DOCUMENTED — WiFi password write‑only (intentional) |
| C6 | 📋 DEFERRED to 1.4 — game-score commands; documented as planned |
| C7 | ✅ DOCUMENTED — hardware settings stay read‑only |
| C8, C10–C14, C17 | ✅ DOCUMENTED — protocol-doc gaps filled |
| C13 | ✅ DOCUMENTED — parameter set is build-dependent |
| C‑VER | ✅ DOCUMENTED (compat rule); 📋 `GET capabilities` deferred to 1.4 |
| C15 | ⏳ OPEN — optional input-length cap (plan Phase 4) |
| C‑ERR-HANDLING, C‑BRACE | ⏳ OPEN — config-tool robustness (plan Phase 3) |

See [`RESOLUTION_PLAN.md`](RESOLUTION_PLAN.md) for the phased execution and [`conflicts.md`](conflicts.md)
for the live status table.
