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

## Architecture: two clip mechanisms

- **Whole-phrase clips** (one clip each): menu entries, preference labels
  (`spokenName`/`parName`), option values, action labels. Played directly.
- **Composed atoms** (sequenced at playback by the voice engine): numbers
  (`0..60` + multiples of 5 to 250), NATO letters, `pro sign`, `error`,
  punctuation names, unit words (`words per minute`, `Volume`, `char`, `Snapshot`,
  `millivolts`). Prosigns, snapshot readouts, Koch "NN char X", voltages, the
  WpM/volume HUD are all built by sequencing these.

A generated **manifest** (UI-string / character → ordered clip-slug list) is the
robust replacement for re-implementing slugify on-device; it also resolves the
lossy-slug cases (`0`/`0%`, `+`).

## Phases & status

- **Phase 0 — branch & scaffolding** — ✅ done. `V9.0` branched; 8.2-beta prep
  committed to master as-is; this doc + report under `devdocs/audio-accessibility/`;
  generated clips git-ignored (`Software/src/data/voice/`).
- **Phase 1 — spokenName field + clip regeneration (incl. characters)** — 🚧 in progress.
  - ✅ `parameter.spokenName` field added; 20 cryptic preference labels populated;
    **both variants build** (Pocket flash +109 B only).
  - ⏳ Move tooling into `Software/tools/audio-accessibility/`; extend the extractor
    for `spokenName` + menu/extraItems spoken overrides + NATO/punctuation/prosign
    atoms; emit the manifest; regenerate + measure the full clip set.
- **Phase 2 — dev partition + filesystem flash** — ⏳ new env `pocketwroom-audio`
  with a custom partition CSV (shrink OTA app slots → grow SPIFFS ~2.9 MB, keep OTA);
  clips in `Software/src/data/voice/`, flashed with `pio run -e pocketwroom-audio
  -t uploadfs`. (Dedicated read-only `voice` partition deferred to Phase 4.)
- **Phase 3 — playback integration** — ⏳ `MorseVoice.*`: `announce()` +
  `composeNumber()` via the existing `sidetone.playSPIFFSFile()`, interrupt-on-
  encoder-turn, hooked into `menuDisplay()`/`displayValueLine()`; a "Voice Output"
  on/off preference; gated by `CONFIG_AUDIO_A11Y`.
- **Phase 4 — release pipeline & installer** — ⏳ dedicated `voice` partition;
  per-firmware partition-table versioning so the web installer isn't broken
  (`rename_binaries.py` / `dropbox_publish.sh` / `flash-m32pocket.html`); EN+DE manual.

## Reproduce the clip set
```
python3 Software/tools/audio-accessibility/extract_menu_strings.py   # -> string lists + manifest
Software/tools/audio-accessibility/generate_audio.sh                  # -> Software/src/data/voice/*.mp3
```
Re-run whenever menu/preference entries change in a release. Requires `espeak-ng` + `lame`.
