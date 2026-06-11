# CLAUDE.md — Morserino-32 Firmware

This file is read automatically at the start of every Claude Code session.
It contains the always-true facts and hard rules for working on this codebase.
Process guidance (how to approach a feature, refactor, or bugfix) lives in the
project skills; this file is about *what is true* and *what must never be violated*.

> **Status: DRAFT v0.2.** Items marked `TODO(audit)` are to be confirmed or
> filled in during/after the consistency audit. Do not guess these — ask or check.

---

## 1. Project overview

The Morserino-32 (M32) is an ESP32-based Morse code (CW) training device.
Firmware is written in **C++** using **PlatformIO** (developed in VS Code on macOS,
versioned on GitHub). The maintainer is Willi, OE1WKL — an experienced CW operator;
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
  on display or board macros. `TODO(audit): list all existing config flags and
  the canonical place where they are defined.`
- **Every change must be built for both variants before it is considered done.**

`TODO(audit): exact PlatformIO environment names and the canonical build
commands for each variant, e.g.:`

```
pio run -e <classic_env>
pio run -e <pocket_env>
```

## 2. Repository layout

`TODO(audit): fill in the actual directory map. At minimum, document where these live:`

- Core Morse engine (keyer, decoder, Koch tables, word/call sign sources)
- Output layer (`MorseOutput`: display, sound, scrolling)
- Display abstraction (`DisplayWrapper`)
- Menu system and preferences/NVS handling
- Games (Morsel, Radio Cave, … `TODO(audit): complete list`)
- QSO Bot
- ESPNow / multiplayer code
- User manuals (English and German) and the PDF build pipeline

## 3. Hard rules (violations have caused real bugs — do not relearn these)

These were each discovered the hard way. Treat them as invariants:

1. **CW audio playback goes through `MorseOutput::pwmTone` directly.**
   Do not invent alternative tone paths or call lower-level APIs.
2. **Call `MorseOutput::resetTOT()` on every user activity** (button, paddle,
   encoder) in interactive modes, otherwise the timeout watchdog fires
   mid-session.
3. **The CW decoder emits a trailing space.** Any code matching decoded text
   against commands or answers must trim it first.
4. **Prosigns must survive text processing.** `buildCwStream` preserves
   uppercase prosigns; do not lowercase, normalize, or split strings carrying
   prosigns before they reach CW generation.
5. **Never rely on the display library's automatic line wrapping.**
   LovyanGFX auto-wrap caused misplaced characters; wrapping is handled by our
   own code (`DisplayWrapper`), which also handles the leading-space-after-wrap
   case. Use the wrapper, not raw display calls.
6. **Visible scroll lines are governed by the `NoOfVisibleLines` constant**
   (currently 4 on the LCD, 3 on the OLED). Never hardcode line counts.
7. **Call sign generation:** prefix-length filtering must respect
   `maxPfxLen = (maxLength > 0) ? maxLength : 99` semantics (bug fixed in
   `getRandomCall()`); be careful when touching this code.

`TODO(audit): extend this list with any further invariants the audit uncovers.`

## 4. Persistence (NVS) conventions

- High scores, save/resume state, and user preferences are stored in **NVS**.
- `TODO(audit): document the canonical namespace and key-naming scheme
  (current code likely uses more than one — the audit should pick a winner,
  e.g. ` `game.<name>.<item>` `), and the standard pattern for versioning
  saved state so firmware updates don't load incompatible blobs.`
- New features must reuse the documented pattern, never invent a new one.

## 5. Coding conventions

- Language: C++ as used by the Arduino/ESP32 toolchain under PlatformIO.
- Match the style of the surrounding file; do not reformat code you are not
  changing.
- Prefer reusing existing helpers (`MorseOutput`, `DisplayWrapper`, Koch
  filtering, word sources, call sign generator) over writing parallel
  implementations. If a helper is *almost* right, extend it — don't fork it.
- Strings shown to the user on the device are English. `TODO(audit): confirm,
  and document any length limits imposed by the displays.`
- Memory matters: the ESP32 is constrained. Avoid `String` churn in hot paths,
  avoid large stack buffers in deeply nested calls, prefer `PROGMEM`/flash
  storage for large constant tables where the platform supports it.
  `TODO(audit): confirm current practice for large word lists.`

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
