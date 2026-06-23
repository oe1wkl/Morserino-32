#!/usr/bin/env bash
#
# generate_audio.sh -- Morserino-32 audio-accessibility clip generator
#
# Reads menu_strings.txt (one UI string per line) and produces one mono MP3 per
# string under audio/, using espeak-ng + lame with the parameters fixed by the
# design phase. Reruns are incremental: an existing *_male.mp3 is left untouched.
#
# Usage:
#   ./generate_audio.sh                 # default voice set, 32 kbps
#   INPUT=menu_descriptions.txt ./generate_audio.sh   # also voice the help texts
#   BITRATE=24 SR=16 ./generate_audio.sh              # smaller fallback set
#
# Requirements: espeak-ng, lame  (brew install espeak-ng lame)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---- Fixed audio parameters (evaluated on real M32 hardware) ----------------
VOICE="${VOICE:-en-gb}"   # British male
SPEED="${SPEED:-140}"     # espeak -s
PITCH="${PITCH:-50}"      # espeak -p
AMP="${AMP:-180}"         # espeak -a
SUFFIX="${SUFFIX:-_male}" # filename suffix (a 2nd voice set would use e.g. _female)

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

command -v espeak-ng >/dev/null || { echo "espeak-ng not found" >&2; exit 1; }
command -v lame      >/dev/null || { echo "lame not found"      >&2; exit 1; }
mkdir -p "$OUTDIR"

# ---- Filename slug: lowercase, spaces->_, strip non-alnum (keep _) ----------
slug() {
  local s
  s="$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]' | sed 's/%/pct/g' | tr ' ' '_')"
  s="$(printf '%s' "$s" | LC_ALL=C tr -cd 'a-z0-9_')"
  s="$(printf '%s' "$s" | sed -E 's/_+/_/g; s/^_//; s/_$//')"
  printf '%s' "$s"
}

generated=0 skipped=0 empty=0
# NB: filename-slug collisions (distinct texts that map to one file) are reported
# by extract_voice_strings.py in voice_manifest.json -- see "collisions".

while IFS= read -r TEXT || [ -n "$TEXT" ]; do
  [ -z "$TEXT" ] && continue
  base="$(slug "$TEXT")"
  if [ -z "$base" ]; then
    echo "  ! empty slug for: '$TEXT' (skipped)" >&2
    empty=$((empty+1)); continue
  fi
  FNAME="${base}${SUFFIX}"
  OUT="$OUTDIR/${FNAME}.mp3"
  if [ -f "$OUT" ]; then
    skipped=$((skipped+1)); continue
  fi
  # Feed text on stdin (not as an argv operand) so strings that begin with '-'
  # -- e.g. "-. dah dit" -- are not mis-parsed by espeak-ng as options.
  printf '%s' "$TEXT" | espeak-ng -v "$VOICE" -s "$SPEED" -p "$PITCH" -a "$AMP" -w "$TMP/${FNAME}.wav"
  lame -m m -b "$BITRATE" --resample "$SR" -q 2 --silent "$TMP/${FNAME}.wav" "$OUT"
  rm -f "$TMP/${FNAME}.wav"
  generated=$((generated+1))
done < "$INPUT"

# ---- Summary ----------------------------------------------------------------
total_kb=$(du -sk "$OUTDIR" | awk '{print $1}')
count=$(find "$OUTDIR" -name "*${SUFFIX}.mp3" | wc -l | tr -d ' ')
echo "----------------------------------------------"
echo "input            : $INPUT"
echo "bitrate          : ${BITRATE} kbps CBR mono @ ${SR} kHz"
echo "generated now    : $generated"
echo "skipped (exist)  : $skipped"
echo "empty-slug skips : $empty"
echo "clips in $OUTDIR/ : $count"
echo "total size       : ${total_kb} KB"
