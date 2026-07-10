# M32 Pocket — Audio Accessibility Feasibility Study

**Goal.** Determine whether the M32 Pocket can speak its main-menu and
preference/option entries as pre-rendered MP3 clips (played on-device), so that a
blind operator no longer needs the serial-protocol + browser-TTS workaround.
**This first step focuses on the memory question**, with the briefing's deliverables
(`menu_strings.txt`, `generate_audio.sh`, `audio/`) used to replace estimates with
**measured** clip sizes.

Date: 2026-06-23 · Target build: `env:pocketwroom` (ESP32-S3, 8 MB flash, TLV320 I2S codec)

---

## 1. Verdict

**Feasible on the existing 8 MB hardware — but not without repartitioning, and not
"for free."** Three findings drive this:

1. **The audio fits the flash chip, not the current partition.** The full set of
   spoken menu/preference/option entries is **246 clips = 1.60 MB** at the
   briefing's intended 32 kbps. The M32 Pocket's **SPIFFS partition is only
   1.50 MB** today *and is shared with the file-player text* — so the clips do
   **not** fit as shipped. The 8 MB chip has the room; the partition table does not.

2. **MP3-from-SPIFFS playback already ships and is in production use.**
   `MorseOutput::soundSignalOK()/ERR()` already call
   `sidetone.playSPIFFSFile("/sounds/success.mp3")` (echo trainer), and the games use
   the same path. The Helix decoder + `arduino-audio-tools` + TLV320 I2S are all live.
   The accessibility feature is **"call `playSPIFFSFile()` with the right clip at the
   right moment"** — not new decode work. *(Corrected after maintainer feedback — see
   the Addendum; the original draft wrongly said no playback code existed.)*

3. **The briefing's flash estimate was based on a bitrate bug.** Its sample
   command (`lame --preset voice -b 32`) actually emits **~59 kbps**, not 32 — that
   is why its samples were ~12 KB. Honouring the stated **32 kbps** halves clip
   size to ~6.8 KB average. All numbers below are re-measured at true 32 kbps.

**Bottom line:** the memory requirement *can* be met on current hardware by
shrinking the two OTA app slots to grow SPIFFS (details in §5), keeping OTA
firmware update intact. A second voice or the full help-text set pushes you toward
a no-OTA layout or, cleanly, a 16 MB flash module on a future Pocket revision.

---

## 2. Deliverables in this folder

| File | What it is |
|---|---|
| `menu_strings.txt` | 250 deduplicated, sorted core strings (menu + pref names + option values + actions) |
| `menu_descriptions.txt` | 46 preference help texts (`parDescript`) — optional "verbose" layer |
| `extract_menu_strings.py` | Reproducible extractor; reads the firmware C++ tables, config-aware for the Pocket build |
| `generate_audio.sh` | espeak-ng + lame clip generator (incremental, bitrate-configurable) |
| `audio/` | 246 generated `*_male.mp3` clips (core set, 32 kbps) |
| `extraction_report.json` | Category counts + slug-collision / TTS-ambiguity flags |

Both scripts regenerate from firmware source, so the list/clips can be refreshed
when entries change in a future release (`python3 extract_menu_strings.py && ./generate_audio.sh`).

> **Location note.** These artifacts live *outside* the firmware git repo (next to
> the briefing) to keep ~2 MB of generated binaries out of version control. If the
> feature proceeds, `extract_menu_strings.py` + `generate_audio.sh` belong in the
> repo (e.g. `Software/tools/audio-accessibility/`); the `audio/` clips should be a
> build artifact / release asset, not committed source.

---

## 3. String inventory (source of truth)

The briefing pointed at the outgoing serial JSON. Those JSON values are produced by
iterating three C++ tables, so the strings were extracted from the tables directly:

| Category | Source (firmware) | Count |
|---|---|---|
| Main-menu entries | `MorseMenu.cpp` `menuText[]` | 34 |
| Preference names | `MorsePreferences.cpp` `pliste[].parName` | 46 |
| Option values (mapped) | `pliste[].mapping[]` | 156 |
| Action / snapshot items | `extraItems[]` + `getValueLine()` polished labels | 17 |
| **Core total (unique)** | | **250** |
| Preference help texts | `pliste[].parDescript` | 46 (separate) |

156 option values dominate — e.g. 15 tone pitches, 14 LICW-carousel entries, the
echo-speed and length ladders. (The briefing assumed ~150 *total*; the real menu
vocabulary is larger.) Config gating matches the Pocket binary: LoRa-only entries
dropped (`LORA_DISABLED`); Bluetooth, TFT theme, games, QSO-bot and I2S-sound
entries kept.

---

## 4. Measured audio sizes (true MP3 payload bytes)

espeak-ng 1.52.0, voice `en-gb`, `-s 140 -p 50 -a 180`, mono. "True payload" =
sum of file sizes (not `du`, which rounds to 4 KB APFS blocks and overstates by ~30%).

| Set | Clips | @ 32 kbps | @ 24 kbps / 16 kHz |
|---|---:|---:|---:|
| **Core** (menu + names + values + actions) | 246 | **1.60 MB** | 1.22 MB |
| Descriptions (help texts) | 46 | 0.65 MB | ~0.50 MB |
| **Core + descriptions** | 292 | **2.25 MB** | ~1.72 MB |

Per-clip @32k: min 3.2 KB (`On`/`Off`), median 6.2 KB, mean 6.8 KB, max 14.9 KB.
The largest clips are the spelled-out tone pitches (`"466 Hz Bb4"` →
"four hundred sixty-six hertz B flat four", ~15 KB each; 15 of them ≈ 0.2 MB). For
comparison, the briefing's literal (buggy) ~59 kbps command would make the core set
**~3.0 MB**.

On-device, **SPIFFS** (256-byte pages, what this firmware uses) adds only a few
percent overhead — far less than LittleFS with 4 KB blocks would for these small
files. Budget ~1.65 MB on-flash for the 32k core set.

---

## 5. Flash budget — the actual memory question

Current `pocketwroom` partition table (decoded from the build's `partitions.bin`):

| Partition | Size | Note |
|---|---:|---|
| nvs | 20 KB | preferences + scores |
| otadata | 8 KB | OTA slot selector |
| **app0** | **3264 KB** | active firmware (uses **2124 KB**, 65%) |
| **app1** | **3264 KB** | OTA update target — **in use** (see below) |
| **spiffs** | **1536 KB** | files; **shared with player.txt** |
| coredump | 64 KB | |
| **total** | **8192 KB** | 8 MB chip, fully allocated |

**The constraint:** the core clip set (1.60 MB) is larger than the *entire* current
SPIFFS partition (1.50 MB), before counting the user's file-player text. So a
repartition is mandatory.

**OTA is real and limits the options.** `MorseWiFi.cpp` uses the ESP32 `Update`
library (`Update.begin/write/end`) for the "Update Firmw" menu item; it writes to
the *inactive* app slot. **app1 cannot simply be deleted** without removing
WiFi/browser firmware update.

### Repartition options

| Option | Layout (8 MB) | SPIFFS gained | Keeps OTA? | Fits |
|---|---|---:|:---:|---|
| **A — shrink both app slots (recommended)** | app 2560 KB ×2 + spiffs ~2900 KB | **~2.9 MB** | ✅ | Core (1.6) or Core+desc (2.25), single voice |
| B — single app, no OTA | app ~3500 KB + spiffs ~4400 KB | **~4.3 MB** | ❌ | Two voices, or generous headroom |
| C — 16 MB flash (future Pocket rev) | app ×2 ~3 MB + spiffs ~9 MB | **~9 MB** | ✅ | Everything, multiple voices |

**Option A** keeps OTA and fits a single-voice core (and even the help texts) today.
Its catch: firmware is already 2.12 MB and *will grow* once the decoder + playback
code land; a 2.56 MB slot leaves ~0.4 MB headroom. Size the slot with that growth
in mind. If the binary approaches the slot size, you are pushed to Option B or C.
**16 MB flash is the clean long-term answer** and makes multi-voice trivial.

---

## 6. Playback / decode feasibility (beyond memory)

The user framed this as "if and how," so the adjacent risks:

- **Decoder: low risk.** `MP3DecoderHelix.h` + `CodecMP3Helix.h` are already in the
  link graph; proven on this exact TLV320 I2S codec. Streaming a mono 32 kbps file
  from SPIFFS is light.
- **RAM: verify.** Helix MP3 needs ~30–40 KB working RAM. The S3 has 320 KB SRAM and
  **no PSRAM** on this board; the games are already RAM-tight and WiFi reserves ~16 KB.
  Confirm decode buffers coexist with the worst-case (post-WiFi, pre-game) heap.
- **I2S sharing: integration work.** The CW sidetone engine owns the I2S sink. Menu
  speech and sidetone are naturally time-multiplexed (announce on selection; key
  afterwards), so no fundamental conflict — but playback must borrow/return the I2S
  output cleanly and re-assert sidetone settings after.
- **Trigger points:** hook `menuDisplay()` and `displayKeyerPreferencesMenu()` /
  `displayValueLine()` to play the clip for the currently-highlighted item — the same
  places that already emit the serial JSON. The UX must follow `devdocs/UX_CONVENTIONS.md`
  (this is a new interaction mode; propose the convention before building it).

---

## 7. Coverage gaps — dynamically composed text

The 250 static strings cover navigation and option selection, but part of the
preference UI is **composed at runtime** and has no fixed clip:

- Snapshot numbers (`"Snapshot 1".."Snapshot 8"`, `Cancel Recall/Store`, `NO SNAPSHOTS`)
- Koch lesson readout (`"NN char X"`), voltage (`"1234 mV"`), LoRa freq/power (N/A on Pocket)
- The live **speed (WpM)** and **volume** HUD values (5–55, 0–20)

To voice these you need ~60–70 atomic clips (digits 0–60, "Snapshot", "WpM", "char",
unit words) **composed in sequence at playback** (+~0.3 MB) — or accept that these
specific dynamic readouts stay silent. This is a design decision, not a blocker.

---

## 8. TTS quality & robustness flags (the "ambiguous/skipped" deliverable)

The display labels are hand-fitted to a 12-char line and make poor TTS input. The
extractor flags 103 such strings; the genuinely problematic ones:

- **Cryptic abbreviations:** `"Stop<Next>Rep"`, `"CurtisB DahT%"`, `"Confrm. Tone"`,
  `"AutoChar Spc"`, `"l-o: Lsp Muted"`, `"Num+Int+ProS"`. espeak mispronounces `<`, `%`,
  `+`, truncations. → **Voice the `parDescript` (or add a dedicated "spoken label"
  field to the `parameter` struct)** rather than the cramped `parName`.
- **Musical/symbolic option values:** `"233 Hz Bb3"`, `"-. dah dit"`, `".- dit dah"`,
  `"BC2: ar sk ="`. Intelligible-ish but odd; consider curated overrides.
- **Filename-slug collisions** (briefing's slug scheme): `Off`/`OFF` and `On`/`ON`
  collapse harmlessly (same word); **`0` and `0%` collapse incorrectly** — fixed in
  the delivered set by forcing `0_male.mp3` = "0", but it shows the scheme is lossy.
- **Empty slug:** the `"+"` option value (Generic-Kbd `<AR>`) strips to nothing — **1
  string skipped**, needs a manual name (e.g. `plus_male.mp3`).
- Strings beginning with `-` broke espeak as argv; the script feeds text on **stdin**
  to avoid option-injection.

**Recommendation:** don't have the firmware re-implement the slug function. Have the
generator emit a **manifest** (`string → filename`) consumed at build time. That
resolves collisions, the empty-slug case, and decouples on-device lookup from the
fragile slugify rules.

---

## 9. Recommendation & next steps

1. **Adopt 32 kbps** (the briefing's stated value); ignore its buggy `--preset voice`
   example. 24 kbps/16 kHz is a usable fallback (1.22 MB core) if needed.
2. **Repartition via Option A** (shrink both OTA app slots to ~2.56 MB, grow SPIFFS to
   ~2.9 MB) to keep OTA and fit a single-voice core today — *budgeting for firmware
   growth*. Flag **16 MB flash** as the clean path for the next Pocket hardware rev
   (enables multi-voice + help texts with no tradeoff).
3. **Voice descriptions, not cryptic `parName`s**, for preferences; add a spoken-label
   field or reuse `parDescript`.
4. **Ship a manifest** (string→file) from the generator; don't duplicate slugify in firmware.
5. **Prototype playback** behind a `CONFIG_AUDIO_A11Y` flag: decode one SPIFFS MP3 to
   the TLV320 via the already-linked Helix/audio-tools path, triggered from
   `menuDisplay()`. Verify RAM headroom in the post-WiFi heap state.
6. **Decide on dynamic content** (§7): atomic-number composition vs. silence.
7. Per repo rules: both hardware variants must still build (the feature is
   `CONFIG_TFT`/Pocket-only — gate it so the OLED/classic build is unaffected), and
   EN+DE manuals get an accessibility section when the feature lands.

**This study did not modify any firmware.** It produced the extraction tooling, the
measured clip set, and this analysis to support the go/no-go and the partition decision.

---

# Addendum (after maintainer feedback, 2026-06-23)

Four clarifications from OE1WKL, with corrections and added analysis. Where this
addendum and the body disagree, **the addendum wins**.

## A. MP3 playback already exists — feature is integration, not new decode (corrects §1, §6)

The body wrongly said "no playback code exists yet." In fact MP3-from-SPIFFS playback
is **already shipping and used**:

- `MorseOutput::soundSignalOK()` → `sidetone.playSPIFFSFile("/sounds/success.mp3")`
  and `soundSignalERR()` → `"/sounds/error.mp3"` (echo trainer OK/ERR), with a
  `pwmTone` beep fallback when the file is absent (`MorseOutput.cpp:2102-2143`).
- The games (`MorseGame`, `MorsePileup`) play their own event sounds the same way.
- All gated on `CONFIG_SOUND_I2S` → the accessibility feature is **naturally
  Pocket-only**, and on the OLED/classic build it simply compiles out (or falls back
  to beeps). No `#ifdef` gymnastics needed.

**Impact:** the decode/RAM/I2S-sharing risks in §6 are largely retired — the device
already decodes MP3s from SPIFFS to the TLV320 in production. The work is: call
`playSPIFFSFile()` from `menuDisplay()` / `displayValueLine()`, manage interruption
(stop a clip when the user turns the encoder again), and provision the clips.

## B. Spoken-label field — confirmed (refines §8)

Decision: add a dedicated **spoken-label field** rather than voicing the cramped
`parName` or the long `parDescript`. Concretely, extend `MorsePreferences::parameter`
with a `const char *spokenName` (and a parallel spoken label for `menuText[]` /
`extraItems[]`), authored for the ear (e.g. `parName "Stop<Next>Rep"` →
`spokenName "Stop after each word"`). The extractor then keys on that field, and the
slug/manifest is generated from it — which also dissolves the cryptic-string TTS
problems and most slug collisions at the source. New strings to author ≈ the 46 pref
names + ~34 menu entries + action items (option *values* and number clips are fine
as-is). This is authoring work, not extra flash.

## C. Number ranges must be composed — measured (replaces §7 "optional")

Silence isn't acceptable, so the numeric value-lines are voiced by **pre-rendering
each reachable integer and composing at playback** ("12 char K" = `12` + `char` + `K`).
Reachable ranges in the firmware: **WpM 5–60, Volume 0–19, Koch lesson 1–52,
Max #Words 0–250 step 5, Snapshot 1–8**.

Generated + measured (`numbers_strings.txt`, `audio_numbers/`): **105 clips = 0.59 MB**
@32k (99 integers `0..60` + multiples of 5 to 250, plus `WpM`/`words per minute`/
`Volume`/`char`/`millivolts`/`Snapshot`). Spelled-out numbers are larger than labels
(mean 5.9 KB; "two hundred thirty-five" is long).

**Open item:** the Koch lesson readout also speaks the *new character* (a letter, digit,
prosign, or punctuation). Voicing those is a separate ~40-clip character set (overlaps
with what a blind CW learner wants anyway) — flag for the design, not sized here.

## D. Repartition will break the web installer unless handled — the real remaining work

This is the crux of "how." Two **linked** problems, both touching the installer/release
pipeline (`scripts/release/dropbox_publish.sh`, `flash-m32pocket.html`):

**D1 — Capacity (the partition table is shared across all versions).** The installer
flashes one **`common/partitions.bin` at 0x8000 for every firmware version**, the app
at 0x10000, and `dropbox_publish.sh` publishes **only the app binary**. So changing the
partition scheme is a *fleet-wide* change: a new table would also be flashed when a user
picks an **old** firmware built for the old table. The app offset (0x10000) is unchanged,
but a moved/resized SPIFFS means old firmware reads its filesystem at the wrong offset.

**D2 — Provisioning (there is no filesystem-image flash path at all).** The installer
flashes **no** SPIFFS image; today's `/sounds/*.mp3` reach the device only via **per-file
base64 upload over serial or the WiFi "Upload File" form** (documented in
`M32 Protocol.md:446` precisely for replacing the echo sounds). Pushing **~2.2 MB** of
voice clips that way is slow and a terrible first-run for a blind user.

**Recommended approach:**

1. **Dedicated, read-only `voice` partition** carved from the reclaimed app-slot space —
   *separate from the user `spiffs`* (which keeps `player.txt` + the user's custom
   success/error sounds). The voice partition is flashed as an installer part and never
   touched by the user; the user's SPIFFS is never clobbered. (A whole-SPIFFS image
   flash would wipe `player.txt` — avoid.)
2. **Version the partition table per firmware:** move `partitions.bin` (and the `voice`
   image) out of the installer's shared `SHARED_PARTS` into each version's `parts`, and
   publish them alongside `firmware.bin` (extend `rename_binaries.py` +
   `dropbox_publish.sh` + the manifest builder in `flash-m32pocket.html`). Old versions
   keep the old table; audio-capable versions ship the new table + voice image. This is
   the change that "doesn't break the installer."
3. Ship the voice image as a **selectable/optional part** (audio build vs. plain build),
   so non-accessibility users aren't forced to flash ~2 MB.

Alternative if you don't want installer changes yet: a **bulk uploader** in the config
web app (loop the existing upload over the clip set) — but it still needs the
repartition for capacity, so it only defers D2, not D1.

## E. Revised flash budget (single voice, 32 kbps, measured)

| Component | Clips | Size |
|---|---:|---:|
| Core (menu + spoken labels + option values + actions) | 246 | 1.60 MB |
| Number/unit composition | 105 | 0.59 MB |
| **Single-voice total** | **351** | **≈ 2.19 MB** |
| (+ optional help-text descriptions) | +46 | +0.65 MB |

Fits **Option A** (shrink both OTA app slots → ~2.9 MB for SPIFFS+voice) with ~0.7 MB
to spare, *keeping OTA*. A **second voice (~4.4 MB)** does not fit Option A → Option B
(no OTA) or, cleanly, **16 MB flash** on the next Pocket hardware revision.

## F. Revised next steps

1. Author the **spoken-label fields** (B) and regenerate from them.
2. Build the **`voice` partition + image** and the **per-version partition publishing**
   in the release pipeline + installer (D) — this is the largest and highest-risk task.
3. Wire `playSPIFFSFile()` into `menuDisplay()`/`displayValueLine()` with interrupt-on-
   navigation, behind `CONFIG_AUDIO_A11Y` (A).
4. Implement **number composition** playback (C); decide on the Koch-character set.
5. Verify RAM headroom in the post-WiFi/pre-game heap (unchanged from §6).
6. EN+DE manual section when it lands.
