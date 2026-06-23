#!/usr/bin/env bash
#
# generate_audio.sh -- Morserino-32 audio-accessibility clip generator
#
# Reads voice_strings.txt (one UI string per line) and produces one mono MP3 per
# string under Software/src/data/voice/, using Piper neural TTS (default; the
# redistributable shipping voice) or espeak-ng, then lame. Reruns are incremental:
# an existing *_male.mp3 is left untouched.
#
# Usage:
#   ./generate_audio.sh                          # Piper (alan, en-GB), 32 kbps
#   LENGTH_SCALE=1.3 ./generate_audio.sh         # slower speech
#   TTS_ENGINE=espeak ./generate_audio.sh        # espeak-ng fallback
#   BITRATE=24 SR=16 ./generate_audio.sh         # smaller fallback set
#
# Requirements: lame, plus EITHER Piper (.venv + `pip install piper-tts` + a voice
# model under models/) OR espeak-ng. (brew install lame espeak-ng)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---- Fixed audio parameters (evaluated on real M32 hardware) ----------------
VOICE="${VOICE:-en-gb}"   # British male
SPEED="${SPEED:-140}"     # espeak -s
PITCH="${PITCH:-50}"      # espeak -p
AMP="${AMP:-180}"         # espeak -a
SUFFIX="${SUFFIX:-_male}" # filename suffix (a 2nd voice set would use e.g. _female)

# ---- TTS engine: 'piper' (neural, redistributable; shipping default) or 'espeak' ----
TTS_ENGINE="${TTS_ENGINE:-piper}"
PIPER_BIN="${PIPER_BIN:-$SCRIPT_DIR/.venv/bin/piper}"
PIPER_MODEL="${PIPER_MODEL:-$SCRIPT_DIR/models/en_GB-alan-medium.onnx}"
LENGTH_SCALE="${LENGTH_SCALE:-1.1}"   # piper phoneme length; >1.0 = slower (1.1 = approved rate)

# ---- Encoding ---------------------------------------------------------------
# The design brief lists "32 kbps". NOTE: its example used `lame --preset voice
# -b 32`, but `--preset voice` is an ABR preset that overrides -b and actually
# emits ~56-60 kbps (hence the brief's ~12 KB/clip samples). We honour the stated
# 32 kbps with a true CBR mono encode. Set BITRATE=24 SR=16 for the smaller
# fallback mentioned in the brief.
BITRATE="${BITRATE:-32}"  # kbps, CBR
SR="${SR:-22.05}"         # output sample rate in kHz (espeak emits 22.05 kHz mono)

# Defaults are repo-relative: the V9.0 string list (from extract_voice_strings.py)
# and the SPIFFS data/voice dir that `pio run -e pocketwroom-audio -t uploadfs` flashes.
INPUT="${INPUT:-$SCRIPT_DIR/voice_strings.txt}"
OUTDIR="${OUTDIR:-$SCRIPT_DIR/../../src/data/voice}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [ "$TTS_ENGINE" = piper ]; then
  [ -x "$PIPER_BIN" ]   || { echo "piper not found at $PIPER_BIN (create .venv + pip install piper-tts)" >&2; exit 1; }
  [ -f "$PIPER_MODEL" ] || { echo "piper model not found: $PIPER_MODEL" >&2; exit 1; }
else
  command -v espeak-ng >/dev/null || { echo "espeak-ng not found" >&2; exit 1; }
fi
command -v lame >/dev/null || { echo "lame not found" >&2; exit 1; }
mkdir -p "$OUTDIR"

# ---- Clip id: first 8 hex of md5(text). Must match extract_voice_strings.py so the
#      manifest and the on-disk /voice/<id>.mp3 names agree. SPIFFS caps the full path
#      at 32 chars, so we cannot use the (long, human-readable) UI strings as names.
clip_id() {
  if command -v md5 >/dev/null 2>&1; then printf '%s' "$1" | md5 -q | cut -c1-8
  else printf '%s' "$1" | md5sum | cut -c1-8; fi
}

generated=0 skipped=0 empty=0
# NB: filename-slug collisions (distinct texts that map to one file) are reported
# by extract_voice_strings.py in voice_manifest.json -- see "collisions".

while IFS= read -r TEXT || [ -n "$TEXT" ]; do
  [ -z "$TEXT" ] && continue
  FNAME="$(clip_id "$TEXT")"      # /voice/<id>.mp3 — id maps back to text via the manifest
  OUT="$OUTDIR/${FNAME}.mp3"
  if [ -f "$OUT" ]; then
    skipped=$((skipped+1)); continue
  fi
  # Synthesize WAV. Text on stdin so a leading '-' (e.g. "-. dah dit") is not
  # mis-parsed as an option.
  if [ "$TTS_ENGINE" = piper ]; then
    printf '%s' "$TEXT" | "$PIPER_BIN" -m "$PIPER_MODEL" --length-scale "$LENGTH_SCALE" -f "$TMP/${FNAME}.wav" 2>/dev/null
  else
    printf '%s' "$TEXT" | espeak-ng -v "$VOICE" -s "$SPEED" -p "$PITCH" -a "$AMP" -w "$TMP/${FNAME}.wav"
  fi
  lame -m m -b "$BITRATE" --resample "$SR" -q 2 --silent "$TMP/${FNAME}.wav" "$OUT"
  rm -f "$TMP/${FNAME}.wav"
  generated=$((generated+1))
done < "$INPUT"

# ---- Summary ----------------------------------------------------------------
total_kb=$(du -sk "$OUTDIR" | awk '{print $1}')
count=$(find "$OUTDIR" -name '*.mp3' | wc -l | tr -d ' ')
echo "----------------------------------------------"
echo "input            : $INPUT"
echo "engine           : $TTS_ENGINE${TTS_ENGINE:+ }$([ "$TTS_ENGINE" = piper ] && echo "(alan, length-scale ${LENGTH_SCALE})")"
echo "bitrate          : ${BITRATE} kbps CBR mono @ ${SR} kHz"
echo "generated now    : $generated"
echo "skipped (exist)  : $skipped"
echo "empty-slug skips : $empty"
echo "clips in $OUTDIR/ : $count"
echo "total size       : ${total_kb} KB"
