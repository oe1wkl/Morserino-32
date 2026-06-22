# M32 Config/Backup Tool — Enhancement Suggestions

Grounded in the audit of `Software/Utilities/m32_config_tool.html` (and `m32_file_manager.html`)
against the firmware and protocol. Each item cites the finding that motivates it and is tagged
**[CORRECTNESS]** (fixes wrong/misleading behaviour) or **[NICE‑TO‑HAVE]** (capability/UX). References
`Cn` point at [`conflicts.md`](conflicts.md).

> **Framing finding (the big one).** Despite the briefing calling this a "config/**backup** tool", the
> shipped tool is a **live editor**: it reads each setting into the UI and writes changes straight to
> the device. There is **no export‑config‑to‑file and no import‑from‑file** anywhere in
> `m32_config_tool.html` (grep for backup/export/import/stringify finds only error logging and the
> help‑JSON fetch). So today you cannot capture a device's configuration to disk or restore it onto
> another device / after a reset. Most items below build toward closing that gap.

---

## A. Correctness fixes

### A1 — Surface the firmware's error responses · [CORRECTNESS] · C3, C‑ERR‑HANDLING
`sendAndParse` returns `JSON.parse(resp)` and every call‑site checks only the *success* key
(`if(sp.control)`, `if(data.configs)`, …; `m32_config_tool.html:668‑672` and throughout). An
`{"error":{…}}` reply parses fine but matches no success key, so failures are **silently swallowed**
(at best a generic timeout is logged). Fix: in `sendAndParse`, detect an `error` object and reject
with its message, reading **both** `error.content` (firmware reality) and `error.name` (documented) so
it survives whichever way C3 is decided. Then show that message to the user instead of nothing.

### A2 — Handle the `reset/defaults` reboot · [CORRECTNESS] · C2 · ✅ DONE 2026-06-22
C2 is fixed in firmware (the reset is real and the device reboots to apply it; protocol stays 1.3). The tool
now warns in the confirm dialog that the device reboots and the connection will drop, logs "device is
rebooting", and calls `doDisconnect()` to tear down cleanly so the user can reconnect
(`m32_config_tool.html:796`). (Earlier concern — the tool reporting a false success for a no‑op — no
longer applies.) Possible follow‑up: an *auto*-reconnect after the reboot, but WebSerial re‑enumeration
may require re‑granting the port, so a clean teardown + prompt was chosen as the robust default.

### A3 — Tolerate variant‑dependent / unknown parameters · [CORRECTNESS] · C13
The "General" group hardcodes `Headphone Output` (`m32_config_tool.html:577`), which doesn't exist on
Classic M32 — `get config/headphone output` returns `INVALID PARAMETER`, currently shown as a blank row.
Drive the parameter list from `get configs` (what the device actually has) rather than a hardcoded
list, and when restoring, **skip + report** any parameter the device rejects rather than failing the
batch.

### A4 — Robust response framing · [CORRECTNESS] · C‑BRACE
`waitForResponse` frames by counting `{`/`}` without skipping string contents
(`m32_config_tool.html:654‑666`). A `{` or `}` inside a CW‑memory `content`, WiFi SSID, custom‑char
set, or player call/name would desync the parser (firmware only strips braces from `file/text` /
`file/first line`, not these). Make the brace counter string‑ and escape‑aware, or read until the JSON
parses, so unusual‑but‑valid values can't corrupt the stream.

### A5 — Firmware‑version display · [CORRECTNESS-adjacent] · C1 · ✅ unblocked 2026-06-22
`dev.firmware||'—'` (`m32_config_tool.html:750`) previously always showed "—" because the firmware sent
`""`. C1 is now fixed (firmware reports its real version, e.g. `"8.1"`), so the Firmware card now shows
a real value and the version‑gated behaviour in D is now possible to build.

### A6 — Validate the connection by the device handshake · [CORRECTNESS] · ✅ DONE 2026-06-22
`doConnect()` previously declared **"Connected"** as soon as the serial port *opened*
(`m32_config_tool.html:682`), using the `{"device":{…}}` reply only to *display* device info inside a
swallow-all `try/catch`. Any port opens at 115200 — including a debug console — so connecting to the
wrong port showed a false "Connected" while nothing worked (every tab then sat at "Connect to load…").
Fixed: a connection now requires a parseable `device` reply to `device/protocol/on`; otherwise the tool
logs "No M32 response on this port…", tears down (`doDisconnect()`), and shows "No M32 found". The
success log now also prints the device identity (hardware · firmware · protocol), which doubles as
confirmation of the C1 firmware-version fix. Reliable precisely because of C1.

---

## B. Coverage gap — what a backup cannot capture (axis 18)

What the protocol can and cannot reach, per NVS namespace (cross‑referenced with
`devdocs/mode-matrix.md` NVS inventory and the firmware):

| Stored data | Namespace | Readable? | Writable? | Gap |
|---|---|---|---|---|
| Numeric parameters (`pliste`) | `morserino` | ✓ `get config(s)` | ✓ `put config` | none (by‑name round trip) |
| Speed / volume | `morserino` | ✓ `get control/*` | ✓ `put control/*` | none |
| Koch lesson / custom chars | `morserino` | ✓ | ✓ | none |
| Player call / name | `morserino` | ✓ `get player` | ✓ `put player/*` | none |
| CW memories 1–8 | `morserino` | ✓ `get cw/memory` | ✓ `put cw/store` | none |
| WiFi SSID / trxpeer / selection | `morserino` | ✓ `get wifi` | ✓ `put wifi/*` | none |
| **WiFi passwords** | `morserino` | ✗ (write‑only) | ✓ | **C5 — can't back up** |
| **Brightness / leftHanded / vAdjust / LoRa RF** | `morserino` | ✓ `get hardware` | ✗ (no writer) | **C7 — can't restore** |
| Snapshots 1–8 (contents) | `snapN` | ✓ `get snapshot/<n>` | ✗ (only store‑from‑current) | partial — can read, can't push contents |
| **Invaders high scores** | `m32game` | ✗ | ✗ | **C6 — unreachable** |
| **Morsel scores (`hi`/`hv`/`wlen`)** | `morsel` | ✗ | ✗ | **C6 — unreachable** |
| **Radio Cave save** | `radiocave` | ✗ | ✗ | **C6 — unreachable** |

**Implication for "backup":** a faithful backup today can include parameters, koch/custom chars,
identity, CW memories, and WiFi SSIDs/peers/selection — but **not** WiFi passwords (C5), hardware
calibration (C7), or any game state (C6). Snapshots can be read for inspection but not reconstructed
from host data. The tool/spec should state this honestly rather than imply a complete backup.

---

## C. Round‑trip integrity (axis 19)

- **Parameters round‑trip cleanly** because both read and write use the **numeric** value and match by
  **name** (`get config` → `displayed` is informational; `put config` takes the number;
  `m32_config_tool.html:899‑1025`). No precision loss (all `uint8_t`). Name‑based matching also makes a
  backup portable across firmware builds where indices shift (C13).
- **Asymmetric settings break the round trip:** write‑only passwords (C5) and read‑only hardware (C7)
  cannot make a full read→write→read cycle. A backup/restore feature must mark these explicitly
  (e.g. "passwords not included", "hardware values shown for reference, not restored").
- **Snapshots:** readable (`get snapshot/<n>`) but there is no "push snapshot contents" command —
  restoring a snapshot means applying each parameter then `put snapshot/store/<n>`. Note the snapshot
  `configs[]` omits `Serial Output` and `Time‑out` (C17), so those won't appear in a snapshot backup.
- **Recommendation:** ship a **round‑trip matrix** (like §B) in the tool's help so users know exactly
  what a backup preserves.

---

## D. Versioning / identification (axis 16) · C‑VER, C1

- **Detect firmware/protocol version.** Once C1 is fixed, read `device.firmware` + `device.protocol` at
  connect (already parsed at `m32_config_tool.html:696‑702`) and **record both into any backup file**.
- **Warn on cross‑version restore.** When restoring, compare the backup's recorded version against the
  connected device and warn if major versions differ or if the backup contains parameters the device
  rejects (C13).
- **Version the backup file format itself.** Any export should carry a `formatVersion` + `protocol` +
  `firmware` + `hardware` header so future tools can migrate it. (There is no backup format today — this
  is greenfield.)
- **Forward‑compat parsing.** Treat unknown objects/keys as ignorable and unsupported commands as
  absent (the protocol is additive within a major version) — see the spec §3.

---

## E. Robustness

- **Error handling** — see A1; the single most impactful robustness fix.
- **Timeouts / pacing** — current per‑command timeouts (2–5 s) and ~100 ms spacing are reasonable, but
  failures are mostly swallowed; once A1 lands, distinguish "device said error" from "no reply"
  (`m32_config_tool.html:654‑665`).
- **Large‑blob chunking** — `file/data` uses `CHUNK_SIZE=180` raw bytes (`:568`) → 240 base64 → 180
  decoded, safely under the firmware's 256‑byte ceiling (C14). Keep it; if the ceiling is ever raised,
  negotiate it rather than hardcoding. Surface the firmware's `"Base64 decode error"` instead of
  retrying blindly (`m32_config_tool.html:1369‑1370`).
- **Unbounded input line** (C15) is a firmware‑side risk; the tool already avoids long single lines, but
  a future "restore" that writes long `customchars/set` or `cw/store` values should respect documented
  caps (CW memory 47 chars; identity 8 chars; etc.).

---

## F. Usability — nice‑to‑haves (clearly separated)

- **[NICE‑TO‑HAVE] Full config backup → file.** The headline capability the tool's name implies but
  doesn't have: `get configs` (+ koch, customchars, player, cw memories, wifi SSIDs/peers/selection) →
  a single versioned JSON file (§D header). Explicitly exclude passwords/hardware/game state with a
  visible note (§B).
- **[NICE‑TO‑HAVE] Restore from file** with **selective restore** (choose which sections), a **dry‑run**
  that lists what would change, and per‑item skip‑on‑reject (C13). Restore parameters by name.
- **[NICE‑TO‑HAVE] Diff device vs backup.** Show which settings differ between the connected device and
  a saved backup before restoring — pairs naturally with the dry‑run.
- **[NICE‑TO‑HAVE] Human‑readable export.** Alongside the machine JSON, a readable report using
  `displayed` values and the help text from `m32_pref_help.json` (already fetched at
  `m32_config_tool.html:1636`) so a backup is also documentation.
- **[NICE‑TO‑HAVE] Snapshot inspection/compare.** `get snapshot/<n>` already returns full contents
  (`m32_config_tool.html:1076‑1083`); add a side‑by‑side of a snapshot vs current settings.
- **[NICE‑TO‑HAVE] Capability probe.** Until a `GET capabilities` exists (spec §3), the tool can probe
  optional commands once at connect (e.g. `get battery`, `get hardware` LoRa fields) and hide UI for
  what the variant doesn't support, instead of showing blank rows.

---

## Priority ordering (suggested)

1. ~~**A2** (reset reboot/reconnect)~~ — ✅ done 2026-06-22.
2. **A1** (surface errors) — unblocks every other diagnosis. [CORRECTNESS]
3. **A3 / A4** (variant‑safe params, robust framing) — correctness under real data. [CORRECTNESS]
4. **F backup/restore + D versioning** — deliver the "backup" the tool is named for. [NICE‑TO‑HAVE]
5. Remaining nice‑to‑haves (diff, human‑readable export, snapshot compare, capability probe).

The remaining correctness items (A1, A3, A4) are pure host‑side changes and do **not** depend on the
firmware decisions in `conflicts.md`. C1 (firmware version) is now fixed, so the backup/versioning work
(D/F) can record a meaningful version whenever it is taken on.
