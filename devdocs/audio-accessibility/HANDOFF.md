# M32 Pocket Audio Accessibility — Handoff / Quick Resume

**Branch `V9.0`** (off master after the 8.2-beta prep). Read this first when resuming;
[`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) has the phase detail and
[`FEASIBILITY_REPORT.md`](FEASIBILITY_REPORT.md) the original analysis.

## Where it stands (2026-06-24)

A working M32 Pocket "Accessibility" firmware that **speaks menu and preference entries**
out loud. Built, flashed, and exercised on real hardware. **Next input: feedback from a
blind user** (planned within days) — that drives prioritisation from here.

**Works on device:**
- Menu entries announced as you scroll; preferences announced as **"heading + value"**
  (e.g. "Serial Output Nothing"); **value-only** while adjusting a value.
- Async, non-blocking, debounced (120 ms settle); **latest entry wins**, no mid-clip interrupt.
- 60 maintainer pronunciation overrides applied (`spoken_overrides.tsv`).

**Known-open (see "Open items"):** a *very-late* freeze on marathon scrolls; audio level/
"clipping"; char-by-char + composed numbers not wired; action-pref labels; release/flash site.

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

1. **Blind-user feedback** — the immediate next step; will reprioritise everything below.
2. **Very-late freeze** — only on a long continuous scroll (hundreds of entries, no pause).
   It's a *slow* resource leak from reusing the MP3 decoder. **Decoder `end()/begin()` reset
   is a dead end — it crashes even at idle** (tried + reverted; `resetDecoder()` is left in the
   lib but unused). Next approach: **heap diagnosis** — add a free-heap serial print, scroll
   ~100 entries, see if heap declines (leak, fixable) or is flat (decoder-internal → would need
   periodic re-init of the whole sidetone engine). Don't keep guessing at the decoder.
3. **Audio level / "clipping"** — *not* file clipping (peaks ~-2 dBFS); the device speaker amp
   adds +6 dB (`setSpeakerGain(6.0)` in `MorseOutput::soundEnableSpeaker`), so a hot clip
   overdrives it. `GAIN_DB` in `generate_audio.sh` (now 3) is the knob — **tune on device**
   (raise if too quiet, lower if it distorts; or lower the amp gain). Clips-only change.
4. **Char-by-char voicing** (Koch new-character, decoder output) — manifest has the data
   (`characters` = NATO-phonetic / "pro sign" sequences); the playback hook isn't wired.
5. **Composed numbers / snapshots** — the `MorseVoice` queue (`announce`+`announceMore`)
   already plays sequences; wire number/snapshot readouts to it (atoms exist: 0..60, units).
6. **Action-pref labels** (snapshots, Calibrate, etc.) — currently value-only; headings TBD.
7. **Phase 4 — release + user-friendly flash site** — publish the accessibility build as its
   own installer entry with its own `partitions.bin` + SPIFFS image (mainline partition is
   untouched, so no fleet migration). See `IMPLEMENTATION_PLAN.md` Phase 4.

## Gotchas that cost time (don't relearn)

- **Clip format is hardware-locked: 44100 Hz / 16-bit / stereo** to match
  `sidetone.begin(44100,16,2)`. The decode path does not resample — 22.05 kHz/mono played 4×
  fast and froze. `generate_audio.sh` encodes via ffmpeg `-ar 44100 -ac 2`.
- **SPIFFS caps the full path at 32 chars** → clips are named `/voice/<8-hex md5 id>.mp3`;
  the firmware resolves strings→ids via `voice_clips.h`, never re-deriving names on device.
- **Async playback must not reset the decoder mid-play or right after** — it races the
  high-priority audio task and crashes. `stopClip()` only hands the mixer back + closes the
  file (the proven `playSPIFFSFile` teardown); `serviceClip()` stops the instant the file is read.
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
silence + overrides · value-only adjust. (`git log --oneline master..V9.0`)
