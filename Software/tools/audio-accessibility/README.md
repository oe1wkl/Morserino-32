# M32 Pocket — audio-accessibility clip tooling (V9.0)

Generates the on-device voice clips for the **M32 Pocket Accessibility** firmware.
Design + rationale: [`devdocs/audio-accessibility/IMPLEMENTATION_PLAN.md`](../../../devdocs/audio-accessibility/IMPLEMENTATION_PLAN.md).

## Files
- `extract_voice_strings.py` — reads the firmware tables (config-aware for the Pocket
  build) and emits `voice_strings.txt` (one clip per line) + `voice_manifest.json`
  (phrase→clip-id, and character/prosign/number → ordered clip-id list for on-device
  composition). Re-run when menu/preference entries change.
- `generate_audio.sh` — renders one mono MP3 per line into `Software/src/data/voice/`.
- `voice_strings.txt`, `voice_manifest.json` — committed build inputs (regenerable).
- `.venv/`, `models/` — **git-ignored** (large / machine-specific); recreate as below.

## One-time setup (Piper neural TTS — the shipping voice)
```bash
cd Software/tools/audio-accessibility
python3 -m venv .venv
.venv/bin/pip install piper-tts
mkdir -p models
BASE=https://huggingface.co/rhasspy/piper-voices/resolve/main/en/en_GB/alan/medium
curl -fsSL -o models/en_GB-alan-medium.onnx      $BASE/en_GB-alan-medium.onnx
curl -fsSL -o models/en_GB-alan-medium.onnx.json $BASE/en_GB-alan-medium.onnx.json
brew install lame            # encoder (espeak-ng optional, fallback engine)
```

## Regenerate
```bash
python3 extract_voice_strings.py
./generate_audio.sh                    # Piper (alan, en-GB), length-scale 1.2, 32 kbps
LENGTH_SCALE=1.3 ./generate_audio.sh   # slower speech
TTS_ENGINE=espeak ./generate_audio.sh  # espeak-ng fallback (no neural model needed)
```
Reruns are incremental (existing `*_male.mp3` are skipped); delete a clip to force it.

**Why Piper, not macOS `say`:** quality is comparable, but Piper's voices are
redistributable — Apple's `say` voices are licensed for local use only and must not
ship in firmware. Because clips are pre-rendered, the engine choice costs no device flash.
