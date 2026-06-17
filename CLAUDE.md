# CLAUDE.md — Morserino-32 Firmware

This file is read automatically at the start of every Claude Code session.
It contains the always-true facts and hard rules for working on this codebase.
Process guidance (how to approach a feature, refactor, or bugfix) lives in the
project skills; this file is about *what is true* and *what must never be violated*.

> **Status: v0.3.** The `TODO(audit)` markers were resolved by the 2026-06
> consistency audit and folded in below. Full evidence and citations live in
> `devdocs/` (`todo-resolutions.md`, `mode-matrix.md`, `divergences.md`).

---

## 1. Project overview

The Morserino-32 (M32) is an ESP32-based Morse code (CW) training device.
Firmware is written in **C++** using **PlatformIO** (developed in VS Code on macOS,
versioned on GitHub). The maintainer is Willi, OE1WKL — an experienced radio amateur, but only an aspiring CW operator (but he usually knows how things are being handled in CW);
correct CW conventions (prosigns, QSO procedure, abbreviations) are a product
requirement, not a nice-to-have.

There are **two hardware variants** that must both be supported by every change:

| | Classic M32 | M32 Pocket |
|---|---|---|
| Display | OLED | LCD via **LovyanGFX**, landscape orientation |
| Notes | original variant | newer, smaller variant |

- Variant-specific behavior is gated by **compile-time config flags**
  (e.g. `CONFIG_QSO_BOT` for the QSO Bot feature). New features that cannot run
  on both variants must introduce such a flag rather than `#ifdef`-ing ad hoc
  on display or board macros. **All config flags are defined per-environment in
  `Software/src/platformio.ini` `build_flags`** (there is no central flags
  header). The master switches are `CONFIG_TFT` (Pocket/LCD vs OLED),
  `CONFIG_CW_GAME` (the four games, TFT-only), `CONFIG_QSO_BOT` (both variants),
  and `LORA_DISABLED`; the full list is in `devdocs/todo-resolutions.md` §A1.
  A few legacy gates still use bare board macros (e.g.
  `#ifdef ARDUINO_heltec_wifi_kit_32_V3`) — prefer named flags for new code.
- **Every change must be built for both variants before it is considered done.**

The two canonical build commands (run from `Software/src/`, where
`platformio.ini` lives):

```
pio run -e heltec_wifi_lora_32_V2    # Classic M32 (OLED, original Heltec V2, has LoRa)
pio run -e pocketwroom               # M32 Pocket (LCD/TFT, ESP32-S3, default env)
```

Other environments exist (`heltec_wifi_kit_32_V3`, `heltec_wifi_lora_32_V3`,
`pocketwroom-lora`, `pocketwroom-170x240`, `heltec-vision-master-t190`,
`minipcb_lora`, `ESP32_S3_Devkit`), but these two are the variant pair every
change must pass. A pre-build hook (`scripts/clean_ino_dupes.py`) strips stray
`<sketch>.ino N.cpp` duplicates first.

## 2. Repository layout

Firmware source lives in `Software/src/Version 6 and newer/`. Key locations
(full map in `devdocs/todo-resolutions.md` §A3):

- **Core Morse engine** — `m32_v6.ino` (`setup()`, `loop()`, keyer
  `doPaddleIambic()`, generator `generateCW()`/`fetchNewWord()`,
  `echoTrainerEval()`); decoder `MorseDecoder.cpp/.h`; tone detect
  `goertzel.cpp`; game CW engine `MorseCwEngine.cpp/.h`.
- **Koch tables + word/call sources** — `Koch` class in `MorsePreferences.*`;
  `english_words.h`, `abbrev.h`, `callsign_prefixes.h`; `getRandomCall()` in
  `m32_v6.ino`.
- **Output layer** — `MorseOutput.cpp/.h` (display, sound/`pwmTone`, scrolling,
  TOT).
- **On-device text entry** — `MorseTextEntry.cpp/.h`: cross-variant encoder
  char-picker (renders via `MorseOutput`, works OLED + TFT). Reuse it for any
  on-device string entry (call/name, WiFi credentials, keyer memories) — don't fork.
- **Display abstraction** — TFT: `oe1wkl/DisplayWrapper` (external lib). OLED:
  `M32OledLGFX.h` (in-tree). `M32PocketLGFX.h` is orphaned dead code (reverted
  #157).
- **Menu / preferences / NVS** — `MorseMenu.cpp/.h`, `MorsePreferences.cpp/.h`;
  serial protocol `MorseJSON.cpp/.h`.
- **Games** (TFT only) — `MorseGame.cpp` (Morse Invaders), `MorsePileup.cpp`
  (Fight the Pileup), `MorseRadioCave.cpp` (Radio Cave), `MorseMorsel.cpp`
  (Morsel); shared infra `MorseGameMode.*`, `GameSprite.*`, `GamePalette.h`.
- **QSO Bot** — `MorseQsoBot.cpp/.h`, `qso_content.h`.
- **ESPNow / WiFi / LoRa** — `MorseWiFi.cpp/.h`; `onEspnowRecv()`/`setupESPNow()`
  and `sendWithLora()` in `m32_v6.ino`; QuickEspNow (pinned fork). Bluetooth:
  `MorseBluetooth.cpp/.h`.
- **User manuals (EN + DE) and PDF pipeline** — `Documentation/User Manual/`;
  filter `Documentation/User Manual/Version 8.x/normalize_ids.lua`.

## 3. Hard rules (violations have caused real bugs — do not relearn these)

These were each discovered the hard way. Treat them as invariants:

1. **CW audio playback goes through `MorseOutput::pwmTone` directly.**
   Do not invent alternative tone paths or call lower-level APIs.
2. **Call `MorseOutput::resetTOT()` on every user activity** (button, paddle,
   encoder) in interactive modes, otherwise the timeout watchdog fires
   mid-session.
3. **The CW decoder emits a trailing space.** Any code matching decoded text
   against commands or answers must trim it first.
4. **Prosigns must survive text processing.** The prosign path is
   `encodeProSigns()` → `generateCWword()` → `cleanUpProSigns()` (all in
   `m32_v6.ino`); it preserves uppercase single-char prosign codes and renders
   them as `<as> <ka> <kn> <sk> <ve> <bk> <err>`. Do not lowercase, normalize,
   or split strings carrying prosigns before they reach CW generation.
   (There is **no** `buildCwStream` function — that name in earlier drafts was
   wrong.)
5. **Never rely on the display library's automatic line wrapping.**
   LovyanGFX auto-wrap caused misplaced characters; wrapping is handled by our
   own code (`DisplayWrapper`), which also handles the leading-space-after-wrap
   case. Use the wrapper, not raw display calls.
6. **Visible scroll lines are governed by the `NoOfVisibleLines` constant**
   (currently 4 on the LCD, 3 on the OLED). Never hardcode line counts.
7. **Call sign generation:** prefix-length filtering must respect
   `maxPfxLen = (maxLength > 0) ? maxLength : 99` semantics (bug fixed in
   `getRandomCall()`); be careful when touching this code.

8. **A game or the QSO Bot must never inherit a transmit `morseState`.** Games
   run under the dedicated `morseGame` state and the bot under `morseQsoBot`;
   both are excluded from every LoRa/WiFi/external-TX gate, so they are
   local-sidetone-only and never key a real rig. Reusing a stale
   `loraTrx`/`wifiTrx` state in a game dereferences torn-down subsystems and
   crashes (see the game dispatch in `MorseMenu.cpp` and the state comments in
   `morsedefs.h`).
9. **`prefPos` additions need three parallel arrays in sync** — the `prefPos`
   enum (`morsedefs.h`), `pliste[]`, and `prefName[]` (`MorsePreferences.*`), all
   positional. A missing `prefName[]` entry is a silent boot-time NVS panic, not
   a compile error. `pliste[]` values are `uint8_t` only — string-valued
   settings (call sign, name, WiFi SSID) need a separate text-entry flow, not a
   `prefPos`. **Non-numeric / action prefs** (text entry, reset, snapshots) go
   *after* `posSerialOut`: they use `extraItems[]` + handler cases
   (`getValueLine`/`adjustKeyerPreference`) + `allOptions`, stay **out** of the
   `pliste[]`/`prefName[]` loop (so no panic), and **must repaint via
   `displayKeyerPreferencesMenu(pos)` after the action** — `setupPreferences` only
   repaints on encoder navigation, so otherwise the screen lingers and the next
   long-press exits the menu. (ClickButton fires a long-press *while held* at
   `longClickTime`, not on release.) On-device text uses the `MorseTextEntry` widget.

## 4. Persistence (NVS) conventions

- High scores, save/resume state, and user preferences are stored in **NVS**.
- **Current reality (four NVS namespaces, kept separate by decision — see
  `devdocs/REFACTORING_PLAN.md` Phase E):** `morserino` (all preferences +
  snapshots + player identity `playerCall`/`playerName`), `m32game` (Morse
  Invaders high scores), `morsel` (Morsel scores `hi`/`hv` + word length `wlen`),
  `radiocave` (Radio Cave save blob). Keys are flat and ad-hoc (`wpm`, `theme`,
  `hi`, `hs0_s`).
- **Each store is self-versioned:** `morserino` has `version_major`/`version_minor`;
  Radio Cave's blob carries `magic`+`version`; Morsel checks an `hv` key; Morse
  Invaders has a `ver` key (added Phase E). An *absent* stamp is treated as the
  current format, so existing data survives upgrades.
- Player identity (`playerCall`/`playerName`) is settable from the **preferences
  menu** (Call Sign / Op Name, added Phase E via the `MorseTextEntry` widget), and
  still inside Fight the Pileup and over serial.
- **For new game state:** prefer reusing an existing namespace over adding more;
  always carry a one-byte version field and treat an absent stamp as current;
  never load an unversioned blob into a changed struct. Full namespace
  consolidation (M5) was deliberately deferred — the stores already self-validate.

## 5. Coding conventions

- Language: C++ as used by the Arduino/ESP32 toolchain under PlatformIO.
- Match the style of the surrounding file; do not reformat code you are not
  changing.
- Prefer reusing existing helpers (`MorseOutput`, `DisplayWrapper`, Koch
  filtering, word sources, call sign generator) over writing parallel
  implementations. If a helper is *almost* right, extend it — don't fork it.
- Strings shown to the user on the device are **English** (confirmed; the only
  non-ASCII is the UTF-8 `©` glyph). Width is bounded by `NoOfCharsPerLine`;
  status-line writes are column-indexed (e.g. the WpM slot at columns 7–12), so
  status strings have hard per-field budgets and the OLED (3 visible lines) is
  the tighter target. Menu labels are hand-fitted (e.g. `"Update Firmw"`).
- Memory matters: the ESP32 is constrained. Avoid `String` churn in hot paths,
  avoid large stack buffers in deeply nested calls. Large constant tables
  (`english_words.h` ~118 KB, `callsign_prefixes.h`) are plain `const` arrays —
  **correct for ESP32**, which places `const` data in flash and reads it through
  the cache, so AVR-style `PROGMEM` accessors are unnecessary (and not used for
  the word lists). Do not add `PROGMEM` wrappers to these tables.

## 6. User-facing consistency

All interactive modes — the classic training modes (CW Keyer, CW Generator,
Echo Trainer, Koch Trainer, Transceiver modes, …) as well as games and the
QSO Bot — must follow **`devdocs/UX_CONVENTIONS.md`**. The classic modes are the
incumbent standard: global mechanisms (speed, volume, mode exit, preferences
entry) are defined by them, and newer features conform to them — not the
other way around. Games (M32 Pocket / TFT only) may differ in screen layout,
never in control mechanisms. Read the conventions before designing any user
interaction.
If a new feature needs an interaction that the conventions don't cover,
propose an addition to that document — do not improvise silently.

## 7. Documentation duties

- The user manual exists in **English and German**; both must be updated for
  any user-visible change, in the same session or with an explicit, written
  TODO handed back to Willi.
- The PDF build uses **pandoc + weasyprint**; heading anchors are normalized
  by `normalize_ids.lua` (umlauts in German headings broke internal TOC links
  before this filter existed). Do not remove or bypass this filter.

## 8. Definition of done (summary)

A change is done when:

1. It builds cleanly for **both** hardware variants.
2. It follows the hard rules in §3 and the NVS conventions in §4.
3. Its user interaction conforms to `devdocs/UX_CONVENTIONS.md`.
4. Manuals (EN + DE) are updated or a TODO is explicitly recorded.
5. No new ad-hoc patterns were introduced where a documented one exists.
