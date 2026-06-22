# M32 Serial Protocol — Conflict List

Every three‑way disagreement found between the **DOC**
(`Documentation/Protocol Description/M32 Protocol.md`), the **FW** (firmware serial layer in
`Software/src/Version 6 and newer/`), and the **UTIL**
(`Software/Utilities/m32_config_tool.html`, `m32_file_manager.html`).

Per the briefing: **no winner is chosen.** Each entry states what each corner does, the nature
and impact of the conflict, and the options — Willi decides per case. Sorted by impact.
`INTENTIONAL?` marks a divergence that may be a deliberate design choice rather than a bug.
`VERIFY-ON-DEVICE` marks behaviour asserted from code that is worth a physical confirmation.

Impact key: **HIGH** = breaks the tool or corrupts/loses config · **MEDIUM** = works but
inconsistent / emits wrong data · **LOW** = cosmetic or doc‑only.

---

## Resolution status (updated 2026-06-22)

Tracked in [`RESOLUTION_PLAN.md`](RESOLUTION_PLAN.md). Decisions ratified by Willi 2026-06-22.
The detailed entries below are the original audit record; this table is the live status.

| # | Disposition |
|---|---|
| C1, C2 | ✅ resolved — firmware (version, reset/defaults) |
| C3 | ✅ resolved — doc aligned to the firmware's `content` key |
| C4 | ✅ resolved — `GET control/volume` min/max swap fixed in firmware |
| C5 | ✅ documented — WiFi password write-only is intentional (security) |
| C6 | 📋 deferred to **1.4** — game-score commands; documented as planned |
| C7 | ✅ documented — hardware settings stay read-only |
| C8, C10, C11, C12, C14, C17 | ✅ documented — protocol-doc gaps filled |
| C9, C16 | ✅ resolved — hardware string + stale examples refreshed |
| C13 | ✅ documented — parameter set is build-dependent; restore tolerates `INVALID PARAMETER` |
| C-VER | ✅ (1) compatibility rule documented · 📋 (2) `GET capabilities` deferred to **1.4** |
| C-NVS-NAMESPACE | ✅ resolved — snapshot/Morsel namespaces fixed in CLAUDE.md + mode-matrix |
| C15 | ⏳ open — optional firmware input-length cap (plan Phase 4) |
| C-ERR-HANDLING, C-BRACE | ⏳ open — config-tool robustness (plan Phase 3) |

After Phases 1–2 the only items left are the two **1.4** deferrals and the optional tool-robustness
work; everything resolvable at protocol 1.3 is closed.

---

## C1 — `GET device` firmware version is always empty · HIGH · ✅ RESOLVED 2026-06-22

> **Resolution (2026-06-22):** Fixed in firmware (option (a)) — `vsn` is now assigned in `setup()`
> from `VERSION_MAJOR.MINOR[.PATCH]`, so `GET device` reports the real version (e.g. `"firmware":"8.1"`).
> Protocol stays **1.3** (fix deemed an implementation detail); DOC examples refreshed. Built clean on both variants; on-device
> confirmation still recommended.

- **DOC** (§"Enabling…", §"Device Information"): the response carries the real firmware version,
  e.g. `{"device":{"hardware":"2nd edition","firmware":"5.0","protocol":"1.0"}}`.
- **FW**: `jsonDevice()` sets `device["firmware"] = vsn` (`MorseJSON.cpp:25`). The global
  `String vsn` is **declared but never assigned** — `vsn`/`brd` are declared at `m32_v6.ino:183`;
  the only version string is built into a *local* `s` for the boot splash (`m32_v6.ino:861‑869`),
  not into `vsn`. (`brd`, by contrast, *is* assigned at `m32_v6.ino:901‑911`.) So every
  `GET device` and every `PUT device/protocol/on` returns `"firmware":""`.
- **UTIL**: `showDeviceInfo()` renders `dev.firmware||'—'` (`m32_config_tool.html:750`) → the
  Firmware card always shows "—". No logic depends on the version (there is none to depend on).
- **Nature**: semantics / missing data. **Impact**: HIGH — it disables any future
  firmware‑version handshake (see C‑VER) and silently misinforms the user.
- **Options**: (a) assign `vsn` once during `setup()` from `VERSION_MAJOR/MINOR/PATCH` (the same
  expression already used for the splash) and update the DOC's stale example; **or** (b) declare
  in the DOC that `firmware` is intentionally blank and have the tool stop displaying it. (a) is
  the obvious fix but is a one‑line *code* change — out of scope this session; recorded here.

## C2 — `PUT device/reset/defaults` does not reset on a configured device · HIGH · ✅ RESOLVED 2026-06-22

> **Resolution (2026-06-22):** Fixed in firmware (option (a), flag+reboot variant) — the serial handler
> now sets NVS `morserino/factoryReset=1`, acknowledges, and `ESP.restart()`s; `setup()` checks+clears
> the flag and runs `resetDefaults()` **before** `readPreferences()`, where `pliste[]` still holds the
> compile-time defaults (the same window the working hardware-menu reset uses). Side effect: the command
> now reboots, so the protocol turns OFF and the host must reconnect — documented in `M32 Protocol.md`
> (the reboot is documented as a 1.3 implementation detail) and handled by the config tool. Built clean on both variants; on-device confirmation of the
> host reconnect still recommended.

- **DOC** (§"Device Information", v1.3): "resets all configurable parameters to their factory
  default values. Snapshots, WiFi configuration, CW memories, and player identity are not affected."
- **FW**: `resetDefaults()` (`MorsePreferences.cpp:1615‑1625`) iterates `i<=posSerialOut` and
  writes the **current in‑RAM** `pliste[i].value` to NVS where it differs. But boot already
  overwrote the compile‑time defaults: `readPreferences()` sets
  `pliste[i].value = <stored>` for every present key (`MorsePreferences.cpp:1449`), and the
  `parameter` struct keeps no separate default copy (`MorsePreferences.h:171‑180`, single mutable
  `value`). So at runtime the loop re‑persists the values that are *already* stored → **no‑op**.
  Defaults are only written for keys *absent* from NVS (fresh device). The same function backs the
  on‑device hardware menu "reset" (`m32_v6.ino:687`), so that path is affected too.
- **UTIL**: `resetDefaults()` (`m32_config_tool.html:796‑803`) confirms *"Reset all parameters to
  factory defaults? Snapshots, WiFi, CW memories, and player identity will be preserved."*, sends
  the command, and logs "Parameters reset to defaults" — reporting success for a no‑op.
- **Nature**: semantics. **Impact**: HIGH — a user "resets to defaults", is told it worked, and
  nothing changes.
- **Options**: (a) fix FW so the command restores compile‑time defaults (e.g. keep a `const`
  default table, or `pref.clear()` the parameter keys then re‑seed from `pliste` initializers via a
  reboot); **or** (b) redefine the command in the DOC as "persist current parameters" and rename/relabel
  the tool button accordingly. Needs Willi's intent: was a real factory reset ever wired up? Confirm
  on hardware.

## C3 — Error object key: `name` (DOC) vs `content` (FW) · MEDIUM

- **DOC** (§"Success and Error Feedback"): `{"error":{"name":"INVALID Value xxx"}}`.
- **FW**: `jsonError(msg)` → `jsonCreate("error", msg, "")` → `obj["content"]=msg`
  (`MorseJSON.cpp:30‑31,147‑153`). The actual wire format is `{"error":{"content":"…"}}`.
  (The success object `{"ok":{"content":"OK"}}` *does* match the DOC.)
- **UTIL**: never reads either key — `sendAndParse` parses the JSON and call‑sites test only for the
  *success* key (e.g. `if(sp.control)`, `if(hw.hardware)`); an error object falls through silently
  (see also C‑ERR‑HANDLING below). So the tool is unaffected, but any doc‑following third party
  reads the wrong field and shows `undefined`.
- **Nature**: response format. **Impact**: MEDIUM (HIGH for any client that surfaces errors).
- **Options**: (a) change the DOC to `content`; **or** (b) change FW to emit `name` (would need a
  dedicated error serializer, since `jsonCreate` is shared with `ok`/`message`). Pick one canonical
  key for the spec.

## C4 — `GET control/volume` returns swapped `minimum`/`maximum` · MEDIUM

- **DOC** (§"Control Speed and Volume"): volume is `0` (no sound) … `19` (max).
- **FW**: `m32Get` calls `jsonControl("volume", sidetoneVolume, volumeMax, volumeMin, true)`
  (`m32_v6.ino:3797`) — the `mini`/`maxi` parameters receive `volumeMax`(=19) and `volumeMin`(=0)
  **in the wrong order** (`volumeMin=0`, `volumeMax=19` at `MorsePreferences.cpp:480‑481`). The
  response is therefore `{"control":{"name":"volume","value":N,"minimum":19,"maximum":0}}`. The
  speed path is correct (`jsonControl("speed", wpm, wpmMin, wpmMax, true)`, `m32_v6.ino:3795`). The
  same swap exists in `changeVolumeValue` (`m32_v6.ino:2617`) but is inert there (`detailed=false`,
  so min/max are not emitted).
- **UTIL**: `loadDashboard()` reads `get control/volume` but consumes **only** `vol.control.value`
  (`m32_config_tool.html:822‑826`); the volume slider's range is hardcoded `min="0" max="19"`
  (line 347). So the shipped tool is unaffected. (Speed, by contrast, *does* read min/max from the
  response, line 816 — and speed is correct.)
- **Nature**: response data (wrong values). **Impact**: MEDIUM — wrong for any client that trusts
  the bounds; latent for this tool.
- **Options**: (a) swap the two arguments in FW (one‑line code fix); **or** (b) document that volume
  bounds are reported reversed (not advisable). Fix is obvious; recorded for a future code session.

## C5 — WiFi password is write‑only (cannot be read back) · MEDIUM · `INTENTIONAL?`

- **DOC** (§"WiFi Configuration"): `GET wifi` returns ssid/trxpeer/selected "but NOT password (!)".
  `PUT wifi/password/<n>/<pw>` sets it.
- **FW**: matches the DOC — `jsonGetWifi()` emits ssid/trxpeer/selected + espnow + wlanChoice, never
  the password (`MorseJSON.cpp:299‑323`); `setPassword()` writes it (`m32_v6.ino:4339`).
- **UTIL**: reads `get wifi` and never expects a password; `setWifiPass()` writes and then clears
  the input field (`m32_config_tool.html:1191`).
- **Nature**: round‑trip integrity (axis 19). **Impact**: MEDIUM — deliberate for security, but it
  means **no backup can ever capture WiFi passwords**; a "restore" silently drops them.
- **Options**: keep as‑is and document the limitation in the spec + tool; **or** add an opt‑in,
  clearly‑guarded export (undesirable). Most likely INTENTIONAL — confirm and document, don't "fix".

## C6 — Game high scores / saves are unreachable by the protocol · MEDIUM

- **DOC**: silent — no command addresses game state.
- **FW**: Morse Invaders scores live in NVS namespace `m32game`, Morsel in `morsel` (`hi`/`hv`/`wlen`),
  Radio Cave in `radiocave` (`save` blob) — see `MorsePreferences.cpp:1130‑1132` (`resetGameScores`)
  and `devdocs/consistency-audit/mode-matrix.md` NVS inventory. **No `GET`/`PUT` reaches any of them**; the serial
  layer only ever opens the `morserino` (and `snapN`) namespaces.
- **UTIL**: cannot read or write game state; no UI for it.
- **Nature**: presence / coverage gap (axis 18). **Impact**: MEDIUM — a "full backup" cannot include
  game progress or high scores. (There is also no protocol `resetGameScores` equivalent, though the
  device menu has one.)
- **Options**: (a) leave games out of protocol scope and state so explicitly; **or** (b) add
  read/clear commands (e.g. `GET game/scores`, `PUT game/scores/clear`) if backup of game state is
  wanted. Decision: is game state in scope for the protocol at all?

## C7 — Hardware settings are read‑only; not restorable · MEDIUM · `INTENTIONAL?`

- **DOC** (§"Hardware Information"): "These values are displayed for informational purposes and
  cannot be changed via the serial protocol" (brightness, leftHanded, vAdjust, and LoRa band/QRG/power).
- **FW**: matches — `GET hardware` exists (`MorseJSON.cpp:434`), there is no writer. These keys are
  **not** in `pliste[]`, so `PUT config/<name>` can't reach them either.
- **UTIL**: shows them read‑only on the Connection tab (`m32_config_tool.html:758‑793`).
- **Nature**: round‑trip integrity (axis 19). **Impact**: MEDIUM — calibration (`vAdjust`),
  `brightness`, `leftHanded`, and LoRa RF config can be read for a backup but **cannot be restored**.
- **Options**: keep read‑only (these are calibration/region values you rarely want to push blindly)
  and document the limitation; **or** add guarded writers (e.g. `PUT hardware/vAdjust/<n>`). Likely
  INTENTIONAL for safety — confirm and document.

## C8 — Terminator described as "carriage return" but is actually LF · LOW

- **DOC** (§"Sending commands"): "Commands are ended by a single carriage return (\n or ascii 10)."
  This is self‑contradictory: **CR is ASCII 13**; `\n` / ASCII 10 is **LF (newline)**.
- **FW**: the parser fires on `inChar == '\n'` (ASCII 10) (`m32_v6.ino:3699`); a leading/trailing
  `\r` is tolerated because the line is `trim()`‑med (`m32_v6.ino:3705,3738`).
- **UTIL**: `sendLine(t)=writer.write(t+'\n')` (`m32_config_tool.html:595`, `m32_file_manager.html:281`)
  — sends LF only.
- **Nature**: doc wording. **Impact**: LOW (everyone actually uses LF; only the *word* is wrong).
- **Options**: correct the DOC to say "LF (`\n`, ASCII 10); a trailing CR is tolerated."

## C9 — Hardware string format differs from the DOC examples · LOW

- **DOC**: `"hardware":"2nd edition"` / `"1st edition"`.
- **FW**: emits `"M32 1st edition"` / `"M32 2nd edition"` / `"unknown M32 board"` for original M32
  (`m32_v6.ino:907‑911`), or `STR(HW_NAME)` for other boards (e.g. the Pocket) (`m32_v6.ino:901`),
  or `"Unknown Device"`. So the real strings carry an `M32 ` prefix or a board macro name, not the
  DOC's bare "Nth edition".
- **UTIL**: displays the string verbatim (`m32_config_tool.html:750‑752`) — no parsing, so robust.
- **Nature**: doc example vs reality. **Impact**: LOW. **Options**: update the DOC examples to the
  real strings, and enumerate the possible values (incl. Pocket's `HW_NAME`).

## C10 — `GET controls` (plural) is undocumented · LOW

- **DOC**: documents only `GET control/speed` and `GET control/volume`.
- **FW**: also implements `GET controls` → `{"controls":[{name,value}×2]}` (`m32_v6.ino:3801`,
  `jsonControls` `MorseJSON.cpp:179`).
- **UTIL**: does not use it.
- **Nature**: presence (FW‑only). **Impact**: LOW. **Options**: document it, or drop it.

## C11 — `PUT wifi/select/0` selects EspNow but is undocumented · LOW

- **DOC**: `PUT wifi/select/<n>` "selects one of the three WiFi entries" — implies n=1..3.
- **FW**: accepts `n=0` → `useEspNow=true` (`selectWifi(0)`, `m32_v6.ino:4382`); the range guard is
  `0..3` and 0 is valid only for `select` (`m32_v6.ino:4133‑4136`). `GET wifi` already exposes
  `espnow`/`wlanChoice` so a client can observe the EspNow state.
- **UTIL**: passes whatever index the UI provides to `put wifi/select/<n>` (`m32_config_tool.html:1193`).
- **Nature**: presence / undocumented value. **Impact**: LOW. **Options**: document `select/0 = EspNow`.

## C12 — `PUT device/protocol/off` response is undocumented · LOW

- **DOC**: doesn't specify a response for `protocol/off`.
- **FW**: emits `{"end m32protocol":{"content":"Goodbye!"}}` (`m32_v6.ino:3907`).
- **UTIL**: sends it on disconnect and ignores the reply (`m32_config_tool.html:720`).
- **Nature**: undocumented response. **Impact**: LOW. **Options**: document the farewell object (and
  consider whether an object named `end m32protocol` — with a space — is the shape you want to bless).

## C13 — `GET configs` parameter set is variant‑dependent and undocumented · LOW · `INTENTIONAL?`

- **DOC**: presents the parameter list as uniform.
- **FW**: the list is data‑driven by compile flags — e.g. `Headphone Output` exists only
  `#ifdef CONFIG_SOUND_I2S` (Pocket) (`MorsePreferences.cpp:101` region), so Classic and Pocket
  expose different parameter sets and therefore different valid `config/<name>` targets.
- **UTIL**: hardcodes `Headphone Output` in its "General" group (`m32_config_tool.html:577`); on a
  Classic M32 `GET config/headphone output` returns `INVALID PARAMETER`, which the tool does not
  surface (it just shows nothing for that row).
- **Nature**: presence varies by build. **Impact**: LOW (works, but a cross‑variant restore drops
  unknown params silently). **Options**: document that the parameter set is build‑dependent and that
  restore must tolerate `INVALID PARAMETER` per item; have the tool report skipped params.

## C14 — `file/data` chunk size has three different ceilings, none negotiated · LOW

- **DOC** (§"Other files"): "chunks can be up to around 200 bytes of binary data."
- **FW**: decodes into a 256‑byte stack buffer, passing `sizeof(decoded)` to
  `mbedtls_base64_decode` (`m32_v6.ino:4060‑4062`) — so a chunk decoding to >256 bytes is **rejected**
  with `{"error":{"content":"Base64 decode error"}}`, not overflowed (safe). Effective ceiling: 256
  decoded bytes.
- **UTIL**: both tools send `CHUNK_SIZE=180` raw bytes (`m32_config_tool.html:568`,
  `m32_file_manager.html:173`) → 240 base64 chars → 180 decoded. Comfortably under 256.
- **Nature**: payload limits (axis 6) — aligned today, but a soft/undocumented limit with an opaque
  error if exceeded. **Impact**: LOW. **Options**: state the exact ceiling (256 decoded / ~344 base64
  chars) in the spec, and make the error message name the limit.

## C15 — No bound on the host→device input line length · LOW

- **FW**: `serialEvent()` accumulates bytes into `String inputString` until `\n`, with **no maximum
  length check** (`m32_v6.ino:3692‑3702`). A pathologically long single line (huge
  `file/append`/`customchars/set`/`cw/store` payload) grows the String on a RAM‑constrained ESP32.
- **UTIL**: never sends very long single lines (uploads are chunked; `cw/store` is truncated to 47).
- **Nature**: robustness (axes 6, 13). **Impact**: LOW in practice. **Options**: cap `inputString`
  and emit an error past the cap; document the maximum command length.

## C16 — DOC examples advertise stale firmware/protocol values · LOW

- **DOC**: examples show `"firmware":"5.0"` and `"protocol":"1.0"` (e.g. §"Enabling…").
- **FW**: protocol is `M32P_VERSION "1.3"` (`morsedefs.h:58`); firmware is `8.1.0`
  (`morsedefs.h:43‑45`) — though `firmware` is emitted empty, see C1.
- **Nature**: doc freshness. **Impact**: LOW. **Options**: refresh the examples to 1.3 / 8.1.x once
  C1 is decided.

## C17 — `GET snapshot/<n>` and `GET configs` iterate different parameter ranges · LOW

- **FW**: `GET configs` iterates `i<=posSerialOut` (`MorseJSON.cpp:83`); `GET snapshot/<n>` iterates
  `i<posSerialOut` **and** skips `posTimeOut` (`MorseJSON.cpp:391‑393`) because those two are not
  stored in snapshots. So a snapshot's `configs[]` is a strict subset of the live `configs` list.
- **DOC**: doesn't mention the difference.
- **Nature**: structural / response content. **Impact**: LOW (correct, just undocumented). **Options**:
  note in the spec that `Serial Output` and `Time‑out` are excluded from snapshots.

---

## Cross‑cutting observations (not single‑command conflicts)

### C‑ERR‑HANDLING — the utility does not parse error responses · MEDIUM (UTIL‑side)
`sendAndParse` returns `JSON.parse(resp)` and every call‑site checks only for the *success* key
(`if(devData.device)`, `if(sp.control)`, `if(data.configs)`, …) (`m32_config_tool.html:668‑672` and
throughout). An `{"error":{…}}` reply parses fine but matches no success key, so the failure is
silently swallowed (at most a generic timeout is logged). Combined with C3, the tool would not show
the firmware's error text even if it tried. → see `utility-enhancements.md`.

### C‑BRACE — the utility frames responses by brace‑counting, ignoring strings · LOW→MEDIUM (UTIL‑side)
`waitForResponse` scans for balanced `{`/`}` and returns the first balanced span
(`m32_config_tool.html:654‑666`). It does **not** skip braces inside JSON string values. FW strips
`{`/`}` from `file/text` and `file/first line` for exactly this reason (`MorseJSON.cpp:229,252‑253`),
but **other** string fields are not stripped — a `{`/`}` inside a CW‑memory `content`, a WiFi SSID, a
custom‑char set, or a player call/name would desync the parser. Unlikely input, real fragility.

### C‑VER — there is no protocol/firmware version handshake the tool can act on · MEDIUM (axis 16)
`M32P_VERSION` is only echoed inside the `device` object, whose `firmware` field is broken (C1). There
is no `GET`/negotiation a tool can use to branch behaviour across firmware versions, and no version on
the (nonexistent) backup file format. → see `utility-enhancements.md`.

### C‑NVS‑NAMESPACE — snapshots live in `snapN`, not `morserino` · INFO (out of strict scope)
`GET snapshot/<n>` opens NVS namespace `"snap"+n` (`MorseJSON.cpp:366`) and `doReadSnapshot`/
`doWriteSnapshot` use `snap0..snap7` (`MorsePreferences.cpp:1653,1683`). CLAUDE.md §4 and
`devdocs/consistency-audit/mode-matrix.md` describe snapshots as part of the `morserino` namespace; the real storage is
per‑snapshot namespaces. Flagged for the NVS docs, not the protocol.
