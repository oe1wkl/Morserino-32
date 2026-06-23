# M32 Pocket — Audio Accessibility (V9.0) — Implementation Plan

Make the M32 Pocket usable by blind operators by speaking menu / preference /
option entries (and composed numbers / characters) as pre-rendered MP3 clips
played on-device. Background, measurements and the flash/partition analysis are in
[`FEASIBILITY_REPORT.md`](FEASIBILITY_REPORT.md) (same folder).

Branch: **`V9.0`** (off master after the 8.2-beta prep commit). The feature is
**M32 Pocket only** (gated by `CONFIG_SOUND_I2S`); the classic OLED build compiles
it out. **Both variants must keep building** every phase.

## Decisions (maintainer, 2026-06-23)

1. **Spoken label = dedicated field**, not the cramped `parName` or the long
   `parDescript`. Implemented as `parameter.spokenName` (nullptr ⇒ fall back to
   `parName`); only the cryptic entries set it.
2. **Letters = NATO phonetic** when a single character is spoken (a→Alpha … z→Zulu).
3. **Prosigns = `"pro sign"` + phonetic letters, composed from snippets**
   (`<ka>` = `pro sign` + `Kilo` + `Alpha`). Reuses the 26 phonetic letter clips, so
   no per-prosign clip is stored — same idea as `Snapshot` + number.
4. **Voice = Piper neural TTS** (`en_GB-alan-medium`, redistributable license),
   generated **offline** with `--length-scale ≈1.2` (slightly slower). Because we
   pre-render, synth quality is decoupled from on-device size — espeak's weak
   sibilants were the real quality issue, not the bitrate. espeak-ng stays as a
   fallback engine (`TTS_ENGINE=espeak`).
5. **Ship as a separate "M32 Pocket Accessibility" binary** (own build env + partition),
   *not* a repartition of the mainline Pocket firmware. The games and the WiFi-AP
   firmware-update/upload are stripped — unusable blind, and updates/uploads go via
   the USB-serial browser tool (screen-reader friendly). This frees the 2nd OTA app
   slot + game flash for a dedicated, larger **voice** store, and — crucially — leaves
   the mainline partition table untouched, so the web installer needs **no fleet-wide
   migration**. Serial-protocol upload stays (config tool); only the WiFi-AP path goes.

## Architecture: two clip mechanisms

- **Whole-phrase clips** (one clip each): menu entries, preference labels
  (`spokenName`/`parName`), option values, action labels. Played directly.
- **Composed atoms** (sequenced at playback by the voice engine): numbers
  (`0..60` + multiples of 5 to 250), NATO letters, `pro sign`, `error`,
  punctuation names, unit words (`words per minute`, `Volume`, `char`, `Snapshot`,
  `millivolts`). Prosigns, snapshot readouts, Koch "NN char X", voltages, the
  WpM/volume HUD are all built by sequencing these.

A generated **manifest** (`voice_manifest.json`) maps each UI string / character →
ordered **clip-id** list. Clips are stored as `/voice/<8-hex md5 id>.mp3` — SPIFFS
caps the full path at 32 chars (the readable slugs ran to ~48) — so the firmware
resolves strings → ids via the manifest, never re-deriving names on-device.

## Phases & status

- **Phase 0 — branch & scaffolding** — ✅ done. `V9.0` branched; 8.2-beta prep
  committed to master as-is; this doc + report under `devdocs/audio-accessibility/`;
  generated clips git-ignored (`Software/src/data/voice/`).
- **Phase 1 — spokenName field + clip regeneration (incl. characters)** — ✅ done.
  - `parameter.spokenName` field added; 20 cryptic preference labels populated;
    **both variants build** (Pocket flash +109 B only).
  - Tooling in `Software/tools/audio-accessibility/`: `extract_voice_strings.py`
    (reads `spokenName` + menu/action spoken overrides + NATO/punctuation/prosign
    atoms; emits `voice_strings.txt` + `voice_manifest.json`) and `generate_audio.sh`.
  - **Measured: 383 clips = 2.34 MB** @32k (254 phrases + 138 atoms; mean 6.4 KB).
    Slug made lossless for `%` (percentages vs integer atoms); only the intentional
    `Off/OFF`,`On/ON` dedupes remain. Manifest drives prosign/number composition
    (`<ka>`→`pro sign`+`Kilo`+`Alpha`). Clips land in `Software/src/data/voice/` (git-ignored).
- **Phase 2 — separate accessibility build + dev flash** — ✅ done. New env
  `pocketwroom-accessibility` (extends `pocketwroom`): `CONFIG_CW_GAME` off, WiFi-AP
  firmware-update/upload gated off, `CONFIG_AUDIO_A11Y` on. Custom partition CSV:
  single app (~2 MB, games gone, no OTA) + a large **SPIFFS (~5 MB)** holding
  `/voice/` + `player.txt`. Dev flash: `pio run -e pocketwroom-accessibility` then
  `-t uploadfs`. Mainline Pocket partition untouched. (Production refinement: split a
  dedicated **read-only `voice` partition** from the user SPIFFS so a reflash never
  clobbers `player.txt` — needs a custom mount + esptool flash.)
  **Verified:** games stripped → firmware **2.03 MB** in the 2.81 MB app slot (RAM 34%);
  `buildfs` SPIFFS image (385 clips, **2.03 MB** Piper @1.1) fits the 5.06 MB voice store.
  Clips named `/voice/<8-hex md5 id>.mp3` (SPIFFS 32-char path limit); manifest maps text→id.
- **Phase 3 — playback integration** — ✅ done (first cut). `MorseVoice::announce(text)` binary-
  searches the generated `voice_clips.h` (firmware UI string → clip id) and plays `/voice/<id>.mp3`
  via `MorseOutput::playVoiceClip()` → `sidetone.playSPIFFSFile()`. Hooked into `menuDisplay()`
  (menu entries) and `displayKeyerPreferencesMenu()` / `displayValueLine()` (pref label + value),
  all `#ifdef CONFIG_AUDIO_A11Y` (now set on the env). The extractor also emits `voice_clips.h`.
  All three variants build (accessibility flash 2.04 MB; mainline + classic unaffected, announce
  is a no-op there). **Limits / follow-ups:** playback is *blocking* (turn-and-hear; no
  interrupt-on-encoder — the library's stream copier blocks); char-by-char voicing (Koch new
  char, decoder output) + composed snapshot/number readouts not wired yet; numeric values voiced
  only where a clip exists (0–60 + ×5 to 250); always-on (no runtime toggle — dedicated build).
- **Phase 4 — release & installer** — ⏳ much simpler under the separate-binary
  approach: publish the accessibility build as its **own** installer entry/page with
  its **own** `partitions.bin` + SPIFFS image — the mainline shared partition is
  unchanged, so no fleet-wide migration. Add its artifacts to `rename_binaries.py` /
  `dropbox_publish.sh`; an install option on `flash-m32pocket.html`; EN+DE manual.

## Reproduce the clip set
```
python3 Software/tools/audio-accessibility/extract_voice_strings.py   # -> voice_strings.txt + voice_manifest.json
Software/tools/audio-accessibility/generate_audio.sh                  # -> Software/src/data/voice/*.mp3
```
Re-run whenever menu/preference entries change. Requires `lame` + Piper (`.venv` +
`pip install piper-tts` + a model under `models/`, see `README.md`); `espeak-ng` is the fallback.
