# M32 Accessibility Edition — Product Plan

*Written 2026-07-06 (session with the maintainer). This is the productization plan for
turning the working V9.0 audio-accessibility prototype into a released product.
The **technical state of the firmware** (architecture, gotchas, build/flash commands)
lives in `HANDOFF.md` in this directory **on the `V9.0` branch** — read that first when
resuming implementation work. This document deliberately does not duplicate it.*

## What this is

A separately published **M32 Pocket firmware** ("Accessibility Edition") that speaks
menus, preferences and messages aloud for blind operators, using pre-rendered Piper
TTS clips from a large dedicated SPIFFS voice store. Pocket-only (needs the I2S codec);
the mainline Pocket and Classic firmware and their partition layout are untouched, so
the existing installer fleet needs no migration.

**Status at time of writing (V9.0 branch):** menus + preferences spoken (heading+value,
value-only while adjusting), async playback rock-solid (audio-task-owned pipeline,
race-freeze root cause confirmed fixed by flat-heap measurement), micro-speaker EQ'd
clip set, 60 maintainer pronunciation overrides, working dev flash via
`pio run -e pocketwroom-accessibility -t upload` / `-t uploadfs`. First blind-user
session done.

---

## Workstream 1 — Firmware slimming

Goal: remove functions that are unusable or pointless without sight, and reclaim
menu simplicity.

- **Games: already stripped** (the env unsets `CONFIG_CW_GAME`).
- **WiFi Update + WiFi Upload: to do.** Introduce a config flag on the a11y env
  (name TBD, e.g. `CONFIG_NO_WIFI_FLASH`), per the CLAUDE.md flag convention.
  The bulk of the work is **menu surgery**: `menuText[]`/`menuNav[]` are positional
  left/right/up/down tables — follow the existing `LORA_DISABLED` / `CONFIG_QSO_BOT`
  `#ifdef` patterns exactly (see `MorseMenu.cpp`).
- **Keep WiFi Trx / ESPNow** — fully usable by ear, and a key feature for blind operators.
- **Consequence — `player.txt`:** WiFi Upload was the path for player files. The serial
  protocol's file upload (config tool / cdaller trainer) replaces it → document this.
  **Caveat to design around:** re-flashing the voice-pack SPIFFS image *wipes*
  `player.txt` (same partition) → the installer must offer "update firmware only"
  separately from "re-flash voice pack".

## Workstream 2 — Voice coverage completion

What separates "prototype" from "product". In priority order:

1. **Composed numbers for all numeric values.** The atoms (0–60, ×5 to 250, units) and
   the `MorseVoice` announce/announceMore sequence queue exist; the composition logic
   is unwired.
2. **Messages, via the serial-protocol stream** *(maintainer decision 2026-07-03)*:
   the M32 protocol text stream is the **canonical inventory of what should be spoken**
   — it was co-designed with Christoph Daller for exactly this purpose (his browser
   trainer, <https://github.com/cdaller/morserino32-trainer>, does TTS on it). Voice
   what the device already emits over the protocol; do **not** scrape display call
   sites. Mechanism: `MorseVoice::announceIfKnown()` fed from the protocol-emit path;
   **missing-clip keys logged to serial** — this doubles as beta telemetry (see WS 5).
3. **Spoken battery status** — a blind user cannot see the battery icon. Speak it at
   boot and on the low-battery warning. Small effort, high value.
4. **Boot-complete announcement** ("Morserino ready") and one after wake-from-sleep.
5. **Koch new-character announcements** — NATO-phonetic / "pro sign" clip sequences are
   already in the manifest, playback unwired.
6. **Action-pref labels** (snapshots, Calibrate, …) — currently value-only.

## Workstream 3 — Release pipeline + combined web installer

- **Artifact set per a11y release:** bootloader + **its own partition table**
  (`m32pocket_accessibility.csv`) + app + **versioned voice-pack SPIFFS image**.
  Never the shared `common/partitions.bin`. Extend `scripts/release/rename_binaries.py`
  and `dropbox_publish.sh` to publish this set alongside the standard binaries.
- **Combined installer page** *(maintainer request)*: esp-web-tools manifests support
  per-chip builds — Classic (ESP32) and Pocket (ESP32-S3) live in **one manifest** and
  the tool auto-selects by detected chip. Product shape: one page, two buttons —
  **"Standard firmware"** (one manifest, chip auto-resolve) and **"Accessibility
  Edition (Pocket)"** (own manifest with the full a11y artifact set). Selecting a11y
  with a Classic attached fails gracefully (wrong chip family).
- **The installer page itself must be screen-reader accessible** — flagship
  requirement, not styling: semantic HTML, keyboard-only flow, `aria-live` progress
  announcements ("Connecting… 40%… done, device rebooting"). A blind user must be able
  to install without sighted help. The browser's serial-port picker is native chrome
  and already accessible; our page has to match it.
- **Firmware ↔ voice-pack pairing:** clips are content-hash ids, so a firmware update
  with changed strings can silently miss clips. Ship a version stamp in the FS image
  (e.g. `/voice/pack.txt`), check at boot (fallback: beep pattern + screen message),
  and rely on the missing-clip serial log. Installer flashes matching pairs only;
  "firmware only" update is allowed but surfaces the pack-version check.
- **Identity:** the a11y build reports itself in the protocol's `build` field
  (protocol 1.3) so config tools — including the cdaller trainer — can adapt.
- **CI:** generate the voice pack in CI (Piper runs on Linux; clip ids are
  deterministic md5-of-text) — removes the macOS/iCloud generation pain (duplicate
  `… 2.mp3` files, `.sconsign` flakiness) from the release path and makes packs
  reproducible. Add `pocketwroom-accessibility` to the CI build matrix immediately so
  the env cannot rot.

## Workstream 4 — Documentation ("manual editions")

**Decision: single-source manual with build-time editions**, the PlatformIO-environments
model applied to pandoc. The pipeline is already 90% there: `build.sh` enables
`markdown+fenced_divs` and has a Lua-filter slot (`normalize_ids.lua`).

- **Conditional blocks** in the one source file per language:

  ```markdown
  ::: {.only-standard}
  ![The M32 Pocket and its connectors](images/pocket-connectors.jpg)
  :::
  ::: {.only-a11y}
  Hold the device with the encoder knob at the top right. On the left edge, from top
  to bottom: the USB-C connector, the headphone jack, …
  :::
  ```

  Inline variants via spans: `[…see the Games chapter]{.only-standard}`.
- **`edition.lua`** (~20 lines): reads `-M edition=standard|a11y`, drops divs/spans
  marked for the other edition. *Implementation note: pandoc Lua filters process
  `Meta` last — use the two-pass idiom (`return {{Meta=…},{Div=…,Span=…}}`) so the
  metadata is read before blocks are filtered.* Both editions run the filter; the
  default (standard) output stays **byte-identical** to today.
- **`build.sh` gains an edition parameter** (`./build.sh en html a11y` →
  `m32UserManual_v8_en_a11y.html`). The filter runs before TOC/section numbering, so
  numbering and internal links adjust automatically per edition.
- **Discipline rule that keeps single-source maintainable:** conditionals stay
  **coarse** — whole sections, whole figure-blocks — never sentence-level confetti.
  Both variants sit adjacent in the source, so future edits see them together.
- **A11y manual is HTML-first** (screen readers handle pandoc HTML far better than
  weasyprint PDFs); ship the PDF too, link the HTML first.
- **A11y-specific content to write:** textual device/connector/control description
  (replacing images — shortened forms double as alt-text for the standard manual, a
  free win); install chapter (combined installer flow with a screen reader); "features
  not in this edition"; the voice-interaction chapter; player-file upload via serial.
- EN + **DE** both (CLAUDE.md duty); Piper/`en_GB-alan` voice attribution + license
  note; screen-reader-friendly quick-start (plain HTML/text); a paragraph for
  morserino.info.

## Workstream 5 — Beta + QA

- Blind-tester round (the existing tester, ideally 2–3 more from the blind ham
  community) with the **missing-clip telemetry** enabled — the serial log shows which
  unspoken strings people actually hit, so coverage work is demand-driven.
- Regression gates: all three envs build (`pocketwroom-accessibility`, `pocketwroom`,
  `heltec_wifi_lora_32_V2`); mainline behavior unchanged (a11y compiles out); marathon
  menu-scroll stays freeze-free (flat heap via `CONFIG_AUDIO_A11Y_DIAG`); volume/
  settings persistence across reboots.

---

## Suggested sequencing

1. **Firmware strip** (WS 1) — self-contained; finalizes the artifact/partition layout.
2. **Coverage sprint** (WS 2) — driven by the serial-protocol inventory.
3. **Pipeline + installer** (WS 3) — manifests, CI clips, accessible page.
4. **Docs + beta** (WS 4 + 5) — telemetry-driven polish afterwards.

## Open decisions for the maintainer

- Config-flag name for the WiFi-update strip (`CONFIG_NO_WIFI_FLASH`?).
- Version scheme: lockstep with mainline (e.g. `9.0-a11y`) vs. own numbering —
  lockstep recommended.
- Whether the combined installer page *replaces* the two existing pages or sits
  alongside them initially.
- Hosting for the installer page + voice-pack artifacts (current flow: Dropbox publish).

## Explicitly out of scope for v1 (noted for later)

- German voice pack (Piper has de_DE voices; device strings stay English).
- Spoken help texts (`parDescript`).
- Any change to the mainline partition layout (the whole design avoids it).
- Codec-side DRC (TLV320AIC3100 has a DRC block, register-level only in the driver) —
  reserve lever if speech ever overdrives the speaker again after the clip-side EQ.

## Session learnings folded in (2026-07-03 … 07-06) — details in HANDOFF.md (V9.0)

- **Decoder reset is dead in every context** (UI task, idle, audio task): this
  audio-tools version cannot re-`begin()` an `EncodedAudioStream` in a running
  pipeline. Decoder is reused via `setStream()`. Never reintroduce a reset.
- **The freeze class was cross-task pipeline mutation**, fixed structurally by the
  audio-task-owned command mailbox + 2 s no-progress watchdog; confirmed on hardware
  by flat per-clip heap over a marathon scroll.
- **"Clipping" was micro-speaker excursion distortion** from the <250 Hz band, not
  file clipping → clip pipeline now applies HPF 250 Hz (24 dB/oct) + presence
  +2.5 dB @ 3 kHz + 2.5:1 compression (`generate_audio.sh` knobs: `GAIN_DB`, `HPF_HZ`).
- **Volume persistence** was only saved on the vol-button toggle path since V7.0 —
  now also saved on menu return and shutdown (fixed on `V9.0` *and* master).
- **iCloud remains hostile to generation + builds** (duplicate clips, evicted
  library files, `.sconsign`) — the strongest argument for CI-generated voice packs;
  local builds use `PLATFORMIO_BUILD_DIR=/tmp/m32build`.
