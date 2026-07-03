# M32 Pocket Audio Accessibility — Handoff / Quick Resume

**Branch `V9.0`** (off master after the 8.2-beta prep). Read this first when resuming;
[`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) has the phase detail and
[`FEASIBILITY_REPORT.md`](FEASIBILITY_REPORT.md) the original analysis.

## Where it stands (2026-07-03)

A working M32 Pocket "Accessibility" firmware that **speaks menu and preference entries**
out loud. Built, flashed, and exercised on real hardware; first blind-user session held.

**Works on device:**
- Menu entries announced as you scroll; preferences announced as **"heading + value"**
  (e.g. "Serial Output Nothing"); **value-only** while adjusting a value.
- Async, non-blocking, debounced (120 ms settle); **latest entry wins**, no mid-clip interrupt.
- 60 maintainer pronunciation overrides applied (`spoken_overrides.tsv`).

**2026-07-03 rework (commit `9d66535` + clip regen) — NEEDS ON-DEVICE VERIFICATION:**
- **Freeze root cause found & fixed structurally**: every freeze/crash variant came from the
  UI task mutating the decode pipeline while the audio task was in `copier->copy()`. The
  audio task now owns the whole clip lifecycle (1-deep command mailbox; `audioLoop()`/
  `teardownClip()` in the vendored lib); decoder is reset **per clip, inside the audio task**
  (safe there — every *cross-task* reset attempt crashed); plus a 2 s no-progress watchdog.
- **"Clipped" sound diagnosed** (spectrogram of a speaker recording): micro-speaker excursion
  distortion from the <250 Hz band, not file clipping. `generate_audio.sh` now applies
  **HPF 250 Hz (24 dB/oct) + presence +2.5 dB @3 kHz + 2.5:1 compression**; audible-band
  loudness +2 dB vs the old set with *lower* peaks.
- Diag build: uncomment `-D CONFIG_AUDIO_A11Y_DIAG=1` in `platformio.ini` → per-clip heap
  trace on serial (verify the leak is gone: scroll ~100 entries, heap should be flat).

**Known-open (see "Open items"):** verify freeze fix + new EQ on device; message coverage
(serial-protocol texts, decided 2026-07-03); char-by-char + composed numbers; action-pref
labels; release/flash site.

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

## Architecture (one paragraph)

The firmware tables (`menuText[]`, `pliste[]` with the new `parameter.spokenName`) are the
source of truth. `extract_voice_strings.py` emits one MP3 per distinct string + a generated
`voice_clips.h` (firmware UI string → 8-hex clip id) and `voice_manifest.json` (also char/
prosign/number → clip-id *sequences* for composition). On device, `MorseVoice::announce()`
binary-searches `voice_clips.h`, and `tick()` (polled from the menu + preference loops) plays
`/voice/<id>.mp3` via `MorseOutput::voiceStart/Service/Stop` → the **vendored, patched**
`cw-i2s-sidetone` library (`Software/src/vendor/`, a Pocket-scoped `symlink://` dep) which got
non-blocking `startClip/serviceClip/stopClip`. Clips are flashed into a large SPIFFS in a
custom partition (`m32pocket_accessibility.csv`); games + WiFi-AP update are stripped from this build.

## Open items (rough priority)

1. **Verify the 2026-07-03 rework on device** — (a) freeze: marathon-scroll hundreds of
   entries (diag build shows per-clip heap — should be flat now that the decoder is reset
   per clip); (b) sound: the EQ'd clip set (cleaner? loud enough vs CW? `GAIN_DB`/`HPF_HZ`
   in `generate_audio.sh` are the knobs, clips-only change).
2. **Message coverage** *(maintainer decision 2026-07-03)*: **the M32 serial-protocol text
   stream is the canonical inventory of what should be spoken** — it was co-designed with
   Christoph Daller exactly for this (his browser trainer,
   <https://github.com/cdaller/morserino32-trainer>, does TTS on it). Voice what the device
   already emits over the protocol; do NOT scrape display call sites. Sketch: a
   `MorseVoice::announceIfKnown()` fed from the protocol-emit path, missing-clip keys logged
   to serial to harvest coverage gaps during real use; static texts get clips, dynamic parts
   compose via the sequence queue.
3. **Char-by-char voicing** (Koch new-character, decoder output) — manifest has the data
   (`characters` = NATO-phonetic / "pro sign" sequences); the playback hook isn't wired.
4. **Composed numbers / snapshots** — the `MorseVoice` queue (`announce`+`announceMore`)
   already plays sequences; wire number/snapshot readouts to it (atoms exist: 0..60, units).
5. **Action-pref labels** (snapshots, Calibrate, etc.) — currently value-only; headings TBD.
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
