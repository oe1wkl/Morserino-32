# M32 Pocket Audio Accessibility — Handoff / Quick Resume

**On `master`** (the V9.0 branch was merged and retired on 2026-07-22 — master is the single
trunk, and it *is* the Accessibility Edition's source). Read this first when resuming;
[`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) has the phase detail and
[`FEASIBILITY_REPORT.md`](FEASIBILITY_REPORT.md) the original analysis.

## Where it stands (2026-08-14)

A working M32 Pocket "Accessibility" firmware that **speaks menu and preference entries**
out loud. Built, flashed, and exercised on real hardware; first blind-user session held.

**Works on device:**
- Menu entries announced as you scroll; preferences announced as **"heading + value"**
  (e.g. "Serial Output Nothing"); **value-only** while adjusting a value.
- Async, non-blocking, debounced (120 ms settle); **latest entry wins**, no mid-clip interrupt.
- 60 maintainer pronunciation overrides applied (`spoken_overrides.tsv`).

**2026-07-03 rework (commits `9d66535` + `cd97e87` + clip regen) — NEEDS ON-DEVICE VERIFICATION:**
- **Freeze root cause reinterpreted & fixed structurally**: every freeze/crash variant came
  from the UI task mutating the decode pipeline while the audio task was in `copier->copy()`
  — including the historical "very-late freeze", which was most likely the per-clip race
  window (a dice roll per clip), **not** decoder-state accumulation. The audio task now owns
  the whole clip lifecycle (1-deep command mailbox; `audioLoop()`/`teardownClip()` in the
  vendored lib) + a 2 s no-progress watchdog that self-heals any residual wedge.
- **Decoder reset is dead in ALL contexts**: the per-clip in-audio-task `end()/begin()`
  froze after the FIRST clip (tried + removed, `cd97e87`), matching the earlier UI-task and
  idle crashes. This audio-tools version cannot re-`begin()` an `EncodedAudioStream` inside
  a running pipeline. The decoder is **reused via `setStream()`** — the pattern the blocking
  `playSPIFFSFile` has proven for years. Never reintroduce a decoder reset.
- **"Clipped" sound diagnosed** (spectrogram of a speaker recording): micro-speaker excursion
  distortion from the <250 Hz band, not file clipping. `generate_audio.sh` now applies
  **HPF 250 Hz (24 dB/oct) + presence +2.5 dB @3 kHz + 2.5:1 compression**; audible-band
  loudness +2 dB vs the old set with *lower* peaks.
- Diag build: uncomment `-D CONFIG_AUDIO_A11Y_DIAG=1` in `platformio.ini` → per-clip heap
  trace on serial (verify the leak is gone: scroll ~100 entries, heap should be flat).

- **Spoken splash, spoken menu path, no browser-only entries** (2026-08-14, **verified on
  Pocket hardware 2026-08-16**) — three fixes to what a blind operator meets first, at
  power-on:
  * **The boot splash speaks** (`announceSplash()` in `m32_v6.ino`): *"Morserino 32
    accessibility edition, version 9 point 0 beta, battery 4 point 1 volts"* — identity,
    version and the same 50 mV-quantised voltage the screen shows, composed from number atoms
    so no version bump or reading needs a new clip. It plays through the splash and on into
    the menu; **any button or encoder action cuts it** (`MorseVoice::stop()` in `menu_()`),
    so it never stands between the user and the device. The low-battery screen — which is a
    dead end, the device sleeps right after — now says *"battery empty"*. The splash's plain
    `delay()`s became `splashPause()`, which turns `MorseVoice::tick()` while it waits: that
    is what actually starts and advances the clips. Content is ~12 s if heard to the end;
    trim it by deleting lines in `announceSplash()`.
  * **The menu names its path** when the listener cannot already know it: at power-on the
    device can come up deep inside a branch, where a bare *"Call Signs"* said nothing about
    CW Generator vs Echo Trainer. `menuDisplay()` now leads with the ancestors whenever the
    branch changed *and* we did not just descend into it (`a11yMenuParent`/`a11yLastEntry`,
    reset on entering `menu_()` and on leaving the preferences). Scrolling within a level
    stays terse; stepping back up a level names the path again.
  * **`Upload File` / `Update Firmw` are gone from this build** (`CONFIG_AUDIO_A11Y` in the
    `menuNo` enum, `menuN`, `menuText[]`/`menuNav[]`, and the three `case` sites). Both only
    raise a WiFi AP and hand the job to a browser on another device — nothing to hear or
    operate on the M32 itself. Firmware goes over USB. Side effect: menu indices shift, so
    `readPreferences()` now bounds the stored `lastExecuted` against `menuN` (a pointer left
    by a *different* edition of the firmware would otherwise index past `menuNav[]`).
- **Composed value lines wired** (2026-08-13): the dynamic preference values — Koch lesson,
  snapshot slot, practice-set size — were *silent*, because `announce()` only matches a whole
  string and these are assembled at runtime. They are now composed from number / character
  atoms (`MorseVoice::announceMoreChar()` + `voiceCharLookup[]`, newly emitted into
  `voice_clips.h`). A blind operator can now hear which Koch lesson and which snapshot slot
  they are on. Extra-item **headings** are voiced too, and the extractor now reads the
  firmware's real `extraItems[]` instead of a hand-kept list that had drifted from it.

**Known-open (see "Open items"):** verify freeze fix + new EQ on device; message coverage
(serial-protocol texts, decided 2026-07-03); decoder char-by-char; the `posVAdjust`
millivolt readout; release/flash site.

## Build & flash (USB, from `Software/src/`)

```
pio run -e pocketwroom-accessibility -t upload      # firmware
pio run -e pocketwroom-accessibility -t uploadfs    # voice clips (SPIFFS)
```
Both on first install; **firmware-only** changes need just `upload`, **clip-only** changes
just `uploadfs`. Other variants still build: `pocketwroom` (mainline) and
`heltec_wifi_lora_32_V2` (classic OLED) — the feature compiles out (`CONFIG_AUDIO_A11Y`).

## Regenerate / customise clips (from `Software/tools/audio-accessibility/`)

```
python3 extract_voice_strings.py     # firmware tables -> voice_strings.txt, voice_manifest.json, voice_clips.h
./generate_audio.sh                  # -> Software/src/data/voice/*.mp3   (Piper 'alan', 44100/stereo)
```
- **Edit pronunciations:** `spoken_overrides.tsv` (`<firmware string><TAB><spoken text>`).
  Strings are listed in `voice_strings.txt`. If `voice_clips.h` changes, re-`upload` too.
- **Voice/rate:** Piper `en_GB-alan-medium`, `LENGTH_SCALE=1.1`. Setup in
  [`Software/tools/audio-accessibility/README.md`](../../Software/tools/audio-accessibility/README.md)
  (`.venv` + model are git-ignored; recreate per machine).
- Requires `espeak-ng`-free toolchain: **Piper + ffmpeg + lame**.

## Clip loudness: what PR #208 changed (2026-08-22)

PR #208 fixed a TLV320 codec bug — the digital DAC gain sat at **+20 dB**, clipping
everything *inside the chip*, downstream of the clips and upstream of Tone Volume. It
now runs at **+2 dB**. That fix is right, but it hit the voice clips harder than the CW
sidetone, so the clips were re-tuned in the same breath.

**Measured over 100 shipped clips**, modelling the real chain (Helix int16 decode →
InputMixer → VolumeStream 0.7 → codec DAC):

| | before (+20 dB) | after (+2 dB) |
|---|---|---|
| voice clips | **18 % of all samples pinned at full scale** | clean, 0/100 clip |
| voice RMS at the DAC | −4.1 dBFS | −17.4 dBFS (**−13.2 dB**) |
| CW sidetone RMS | −0.2 dBFS | −4.1 dBFS (**−3.8 dB**) |

Speech therefore fell **9.4 dB behind CW**. The retune wins back ~2 dB of that by raising
`GAIN_DB` 6 → 10 (chosen from a sweep — see the comment block in `generate_audio.sh`).

**The rest is not available in the digital domain, and that is not a tuning failure.**
The old loudness came *from* the clipping: a signal with 18 % of its samples flat-topped
is far denser than the same speech peaking cleanly at the same ceiling. At a fixed peak,
RMS is bounded by speech's own ~13 dB crest factor. Pushing `GAIN_DB` higher only
re-creates the distortion that was just removed — **do not "fix" quiet speech that way.**

### Where the levels actually landed (bench-verified 2026-08-22)

After the retune, two more passes on real hardware settled the analog side. All three
constants live together at the top of the audio section in `MorseOutput.cpp`:

| constant | value | effect |
|---|---|---|
| `SIDETONE_LEVEL` | 0.79 | shared post-mixer VolumeStream. The **digital ceiling**: the sine is full scale and the DAC adds +2 dB, so 0.79 × 1.259 = 0.995. Do not raise it. |
| `SPEAKER_TRIM_DB` | 3.0 | speaker runs one gain step up (18 dB) with this trim on the volume control = **+3 dB net** |
| `HEADPHONE_GAIN_DB` | 6.0 | headphone driver gain (valid 0–9), **−3 dB** from the 9 dB PR #208 set |

Net vs. PR #208: CW **+4 dB on the speaker**, +1 dB on headphones; phones sit **6 dB down
relative to the speaker** (3 dB from each side). Verified by ear: speech right, CW right,
speaker/headphone balance right.

**Why CW needed raising at all**, when the measurements said it had got *louder*: the old
sidetone was a clipped **square** wave whose harmonics at 1.8 / 3 / 4.2 kHz sat exactly
where this micro-speaker is efficient and the ear is most sensitive. De-clipping removed
them, so a clean 600 Hz sine reads as quieter on the speaker despite a higher RMS. On
headphones, which reproduce 600 Hz perfectly well, the effect does not apply — which is
why the speaker got +3 dB of analog and the headphone path did not.

**Trim the headphone side on the GAIN, never the volume control.** The volume control is
an attenuator ahead of the driver, so trimming there lowers the signal but not the driver's
own noise, and the floor becomes audible once Tone Volume is turned up to compensate — a
−6 dB volume offset did exactly that during the PR #208 bench work. Driver gain scales
signal and upstream noise together.

If speech alone ever needs to move independently of CW, the analog stage is shared, so the
answer is **a dedicated "Voice Volume" preference** for this edition (a `prefPos` + clips +
manual). Worth doing eventually — blind users differ a lot in what they need, and today one
control moves speech and sidetone together.

**Two things not to undo:**
- **`HPF_HZ=250` stays.** The micro-speaker excursion argument behind it is a separate,
  real, physical effect — and the speaker amp has since gone 6 → 12 → 18 dB, so excursion
  risk went *up* substantially. Relaxing the high-pass adds cone travel far more than loudness
  (the speaker cannot usefully reproduce Alan's ~110 Hz fundamental anyway).
- **The post-encode peak check.** 32 kbps MP3 encoding moves peaks unpredictably (±3 dB
  observed), so a clip can leave the encoder hotter than the pre-encode limiter allowed and
  saturate the *decoder* — 2 of every 100 shipped clips did. `generate_audio.sh` now decodes
  every clip it writes and re-encodes with a trim if it lands over `PEAK_CEILING` (0.95),
  converging over up to 3 passes. The summary line reports how many were trimmed.

Firmware side, `soundSetup()` now pins the shared post-mixer VolumeStream to `SIDETONE_LEVEL`. The
library's `begin()` leaves it at 0.8 and only `pwmTone()` lowered it, so anything played
before the first tone ran 1.2 dB hot — and the boot announcement is the first sound of
every session on this edition.

**Flashing gotcha that cost an hour here:** `pocketwroom` is `default_envs`, so a `pio run
-t upload` without an explicit `-e` flashes the NON-accessibility build — where
`MorseVoice::announce()` is a compiled-out no-op. Symptom: perfect CW, zero speech on both
speaker and headphones, whatever is in SPIFFS. Tell-tale: the CW Games entry is present in
the menu (the a11y build unflags `CONFIG_CW_GAME`). Always pass `-e
pocketwroom-accessibility`, and run `upload` and `uploadfs` as **separate** commands — this
is an ESP32-S3 on native USB and the port re-enumerates on the reboot after `upload`.

## Missing clip store: the boot alarm (2026-08-23)

The clips live in SPIFFS, the firmware in flash, and they come apart easily — flash the
image without `-t uploadfs`, or let `SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)` reformat a
half-written filesystem, and every `announce()` becomes a silent no-op while CW keeps
working perfectly. For this build's audience a mute device and a dead device are the same
device, so boot now checks and reports.

- **Detection** — `MorseVoice::clipStoreOk()`: `SPIFFS.exists()` on 8 ids spread across
  `voiceLookup[]` (the same test `startClip()` makes), cached after the first call. Verdict
  is "gutted" only when **fewer than half** the sample is present. That threshold is
  deliberate: one clip that failed to render must never nag on every boot for the life of
  the device, and a spread sample still catches a truncated upload.
- **Report** — `MorseVoice::warnClipStore()` keys **four high/low tone pairs** (1200/600 Hz,
  180 ms each, ~2.1 s total) through `MorseOutput::pwmTone`, driven by the same `tick()` as
  the clips and cut by the same `stop()`. It runs inside the splash's existing
  `splashPause()` windows, so boot gets no longer. `displayStartUp()` puts
  `clipStoreDisplayText()` ("No voice clips!") on scroll line 2 for a sighted helper, and
  the fix goes to the serial log.
- **Why tones and not CW or speech.** Speech is the thing that is missing. Morse was
  considered and **rejected by Willi**: it assumes CW literacy and a copyable speed, and
  this build's users include complete beginners. 1200 Hz sits above the whole sidetone range
  (`MorseOutput::notes[]` tops out at 932 Hz), so the alarm cannot be mistaken for CW at any
  Pitch setting. Meaning is documented in both manuals ("If the Morserino Stops Speaking" /
  "Wenn der Morserino nicht mehr spricht") — a signal has to be learned, so it must be
  written down.
- **The display line is deliberately NOT voiced**, and must stay that way: a clip for it
  would be missing in exactly the situation it exists to report. This is CLAUDE.md §8's
  documented exception, not a gap — the note is in the code at both call sites so nobody
  "fixes" it later.
- **Pack-version check (added the same day).** Content-hash clip ids mean a firmware whose
  strings changed looks for ids an older pack never held, and goes quiet on exactly those
  entries with nothing to show for it. So the pack now names itself:
  - `extract_voice_strings.py` computes **`PACK_STAMP`** = first 8 hex of
    `md5(each unique clip id, sorted, one per line, trailing newline)` and emits
    `#define VOICE_PACK_STAMP` into `voice_clips.h` (plus `pack_stamp` in the manifest).
  - `generate_audio.sh` writes the same value to **`data/voice/pack.txt`** — computed
    independently from `$EXPECTED` as `LC_ALL=C sort -u | md5 | cut -c1-8`, which is the
    same rule by construction. **Keep the two in step.** It writes the file only when
    every expected clip is actually on disk, and *deletes* it otherwise: a stamp on an
    incomplete pack would claim a completeness it does not have.
  - `probeClipStore()` reads it after the existence probe passes and returns `CV_STALE` on
    a mismatch. **An absent `pack.txt` is treated as fine** — a pre-stamp pack, or one the
    generator would not vouch for; we cannot tell right from wrong, so we do not cry wolf
    (same rule as an absent NVS version stamp, CLAUDE.md §4).
  - Behaviour differs by verdict: `CV_MISSING` → alarm, and the splash is not even
    attempted. `CV_STALE` → alarm, **then the splash is spoken anyway** — most of a
    mismatched pack still plays, and only the entries added since it stay silent.
    `clipStoreUsable()` is that distinction; `clipStoreDisplayText()` returns
    "No voice clips!" or "Wrong voice pack!".
  - Same alarm for both, deliberately: one signal to learn, meaning "my speech is not
    right". The display line and the serial log carry the distinction.

- **Related, same day: the installer left the *other* edition's filesystem behind.** The
  Pocket layouts overlap, so switching a11y → Standard left ~3.6 MB of clips physically
  intact, and a later firmware-only a11y flash resurrected them — a device speaking from a
  pack it was never built for. Full measurements and the clean-on-switch fix are in
  `devdocs/installer/PLAN.md` §12b. The pack stamp above is the belt to that fix's braces.

## Checklist: you added a preference / menu entry / message (CLAUDE.md §8)

Master *is* the Accessibility Edition, so new UI text is mute until it has a clip.

| You added | What it needs |
|---|---|
| Preference, option value, menu entry | Re-run the extractor + generator (above). Cryptic `parName`? add `parameter.spokenName` first. |
| **On-screen message / status response** | **Manual work** — the extractor cannot see display calls. Voice it via the serial-protocol stream (see Open items §2). |
| A phrase you announce from code, in no table | Add it to a list in the extractor (`SPLASH_WORDS` is the precedent — the boot splash is drawn, not table-driven), then re-run extractor + generator. |
| Text inside a game (`CONFIG_CW_GAME`) | Nothing — compiled out of this build. |

**Cheap check:** re-run `extract_voice_strings.py`, then `git diff voice_strings.txt`.
Empty = nothing owed; added lines = clips owed. Flash **both** images afterwards
(`voice_clips.h` is compiled in → `-t upload`; clips live in SPIFFS → `-t uploadfs`).

*Drift precedent (2026-07-06):* the V8.2→V9.0 merge brought 7 unvoiced strings —
`QSO Difficulty` + `Beginner`/`Intermediate`/`Advanced` (a real gap: the QSO Bot ships
in this build) plus 3 game entries. Exactly what this checklist exists to prevent.

## Architecture (one paragraph)

The firmware tables (`menuText[]`, `pliste[]` with the new `parameter.spokenName`) are the
source of truth, plus a few hand-listed phrase groups for text that lives in no table
(`UNIT_WORDS`, `SPLASH_WORDS`). `extract_voice_strings.py` emits one MP3 per distinct string + a generated
`voice_clips.h` (firmware UI string → 8-hex clip id, **plus** `voiceCharLookup[]`: raw
character → clip-id *sequence*) and `voice_manifest.json`. On device, `MorseVoice::announce()`
binary-searches `voice_clips.h`, `announceMoreChar()` linear-scans `voiceCharLookup[]` to spell
a character out, and `tick()` (polled from the menu + preference loops) plays
`/voice/<id>.mp3` via `MorseOutput::voiceStart/Service/Stop` → the **vendored, patched**
`cw-i2s-sidetone` library (`Software/src/vendor/`, a Pocket-scoped `symlink://` dep) which got
non-blocking `startClip/serviceClip/stopClip`. Clips are flashed into a large SPIFFS in a
custom partition (`m32pocket_accessibility.csv`); games and the two WiFi-AP entries
(Upload File / Update Firmw) are stripped from this build.

## Open items (rough priority)

1. **Verify the 2026-07-03 rework on device** — (a) freeze: marathon-scroll hundreds of
   entries; with the diag build the per-clip heap trace should be **flat** (no leak = the
   race-window theory holds; a decline = real accumulation, needs the offline-rebuild
   approach); (b) sound: the EQ'd clip set (cleaner? loud enough vs CW? `GAIN_DB`/`HPF_HZ`
   in `generate_audio.sh` are the knobs, clips-only change).
2. **Message coverage** *(maintainer decision 2026-07-03)*: **the M32 serial-protocol text
   stream is the canonical inventory of what should be spoken** — it was co-designed with
   Christoph Daller exactly for this (his browser trainer,
   <https://github.com/cdaller/morserino32-trainer>, does TTS on it). Voice what the device
   already emits over the protocol; do NOT scrape display call sites. Sketch: a
   `MorseVoice::announceIfKnown()` fed from the protocol-emit path, missing-clip keys logged
   to serial to harvest coverage gaps during real use; static texts get clips, dynamic parts
   compose via the sequence queue.
3. **Char-by-char voicing** — *done for the Koch lesson character* (2026-08-13):
   `MorseVoice::announceMoreChar()` reads the generated `voiceCharLookup[]` (raw firmware
   char → NATO-phonetic / "pro sign" sequence). **Still open: decoder output**, which needs
   the same hook driven from the decode path rather than the preferences menu.
4. **Composed numbers / snapshots** — *done* (2026-08-13) for Koch lesson, snapshot slot and
   practice-set size, via `announceValue()` in `MorsePreferences.cpp`; the boot splash speaks
   the battery the same way (2026-08-14, "4 point 1 volts" — whole volts and tenths are inside
   the integer-atom range). **Still open: the `posVAdjust` calibration readout** ("3980 mV") —
   integer atoms only go to 250, so that one needs a digit-spelling path.
5. **Action-pref labels** — *done* (2026-08-13): extra items announce their heading on entry,
   and the extractor reads `extraItems[]` from the firmware, so a cryptic display label is
   fixed with an `ACTION_SPOKEN` entry ("RECALLSnapshot" → "Recall snapshot").
6. **Phase 4 — release + user-friendly flash site** — publish the accessibility build as its
   own installer entry with its own `partitions.bin` + SPIFFS image (mainline partition is
   untouched, so no fleet migration). See `IMPLEMENTATION_PLAN.md` Phase 4.
7. *(reserve)* **Codec DRC**: the TLV320AIC3100 has a DRC block (register-level only in the
   driver lib) — an option if speech still overdrives the speaker after the clip-side EQ.

## Gotchas that cost time (don't relearn)

- **Clip format is hardware-locked: 44100 Hz / 16-bit / stereo** to match
  `sidetone.begin(44100,16,2)`. The decode path does not resample — 22.05 kHz/mono played 4×
  fast and froze. `generate_audio.sh` encodes via ffmpeg `-ar 44100 -ac 2`.
- **SPIFFS caps the full path at 32 chars** → clips are named `/voice/<8-hex md5 id>.mp3`;
  the firmware resolves strings→ids via `voice_clips.h`, never re-deriving names on device.
- **Never mutate the decode pipeline from outside the audio task** — the audio task runs
  `copier->copy()` at top priority; any UI-task mutation (mixer switch, `setStream`, decoder
  `end()/begin()`) races it → the historical freezes and crashes. Since `9d66535`,
  `startClip/stopClip` only post to a mailbox and ALL pipeline work lives in
  `audioLoop()`/`teardownClip()` (audio-task context). Keep it that way.
- **`volumedetect` reports at log level `info`** — running it under `ffmpeg -v quiet`
  silently returns *nothing*, which reads as "no overshoot" and disables the peak check
  entirely. Use `-hide_banner -nostats`. Cost an hour of a check that looked like it passed.
- **iCloud**: the repo lives under `~/Documents` (iCloud-synced). This causes `… 2.mp3`
  duplicate clip files and flaky/`.sconsign` build failures. Mitigations used: clean dupes
  (keep only `voice_strings.txt` ids), and `PLATFORMIO_BUILD_DIR=/tmp/m32build` for reliable
  builds. **Recommend moving the repo (or at least `.pio` + `data/`) out of iCloud.**
- **After changing lib_deps / the vendored lib**, PlatformIO leaves a stale build graph
  (`m32_v6.ino` "Converted" but not compiled → undefined `setup()/loop()`, or missing `.d`
  files). Fix: `rm -rf .pio/build/<env>` and rebuild.

## Key commits (V9.0)
Phase 0/1 spokenName+tooling · Phase 2 env+partition+id-clips · Piper engine · Phase 3
playback · 44100 format fix · async vendored fork · EOF-stop freeze fix · heading+value +
silence + overrides · value-only adjust · `9d66535` audio-task-owned pipeline + per-clip
decoder reset + watchdog + speaker EQ. (`git log --oneline master..V9.0`)
