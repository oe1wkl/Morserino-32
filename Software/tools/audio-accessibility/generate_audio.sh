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
#   ./generate_audio.sh --prune                  # also delete orphaned clips
#   LENGTH_SCALE=1.3 ./generate_audio.sh         # slower speech
#   TTS_ENGINE=espeak ./generate_audio.sh        # espeak-ng fallback
#   BITRATE=24 SR=16 ./generate_audio.sh         # smaller fallback set
#
# The clips are committed to the repo (they are NOT reproducible from a clean
# clone: the Piper voice model under models/ is gitignored), so a builder who
# only wants to flash does not need the TTS toolchain at all -- just uploadfs.
# Re-run this only when voice_strings.txt changes or the voice/EQ is retuned.
#
# Requirements: lame, plus EITHER Piper (.venv + `pip install piper-tts` + a voice
# model under models/) OR espeak-ng. (brew install lame espeak-ng)
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ---- Args -------------------------------------------------------------------
PRUNE=0
for arg in "$@"; do
  case "$arg" in
    --prune)   PRUNE=1 ;;
    -h|--help) sed -n '3,25p' "$0"; exit 0 ;;
    *) echo "unknown option: $arg (try --help)" >&2; exit 2 ;;
  esac
done

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
# Clips MUST match the M32 Pocket I2S sidetone format: sidetone.begin(44100, 16, 2)
# = 44100 Hz, 16-bit, STEREO (MorseOutput.cpp). The cw-i2s-sidetone decode path does
# NOT resample, so a 22.05 kHz / mono clip plays back 4x too fast (2x rate x 2x channels)
# AND under/over-feeds the copier's bounded result queue, stalling playback after a few
# clips. Encode at 44100 Hz stereo to match the hardware.
BITRATE="${BITRATE:-32}"  # kbps, CBR
OUT_SR="${OUT_SR:-44100}" # Hz       -- must equal the I2S sample rate
OUT_CH="${OUT_CH:-2}"     # channels -- must equal the I2S channel count (stereo)
# Loudness + micro-speaker EQ.
#
# 2026-08-22 UPDATE -- the 2026-07-03 diagnosis below was only half the story. PR #208
# found the REAL dominant cause: the TLV320 codec's digital DAC gain was set to +20 dB,
# which clipped every tone and every voice clip INSIDE the chip, downstream of these
# files and upstream of the volume control. Measured over 100 shipped clips, an average
# 18 % of all samples were pinned at full scale. That is why the file-level analysis
# below correctly found "no flat-topping in the files" and still heard flat-topping on
# the device. The DAC now runs at +2 dB and the path is clean.
# Consequence for THIS chain: it was tuned defensively against that clipping, and the
# de-clipped path is ~13 dB quieter in RMS for speech (vs only ~4 dB for the CW sidetone),
# so GAIN_DB was raised 6 -> 10 (see below). The micro-speaker excursion argument is kept
# -- it is a separate, real, physical effect, and PR #208 also raised the speaker amp gain
# 6 -> 12 dB, so excursion risk went UP, not down. Do not relax HPF_HZ without listening.
#
# ORIGINAL (2026-07-03) analysis, still valid as far as it goes:
# The "clipped" sound on hardware was NOT file clipping (peaks
# ~-2 dBFS, zero flat-topping): spectrogram analysis of a speaker recording (2026-07-03)
# showed vowel harmonics smeared up to 8-16 kHz = the micro-speaker driven past its
# excursion limit. Alan's fundamental is ~110 Hz; energy below ~250 Hz produces no audible
# output on this speaker, only cone excursion (= distortion). So:
#   - HPF_HZ      : cascaded 2x2-pole high-pass (24 dB/oct) removes the distorting sub-band.
#                   This is why CW (a single ~600 Hz tone) always sounded clean while speech
#                   at the same volume distorted.
#   - PRESENCE_DB : gentle bell at 3 kHz for intelligibility.
#   - compressor  : 2.5:1 raises average loudness toward the CW sidetone WITHOUT raising
#                   peaks (the earlier pure-gain approach traded loudness against overdrive).
# EBU loudnorm/dynaudnorm went the WRONG way for this short speech. Tune on-device:
# GAIN_DB louder/softer overall; if it still distorts, raise HPF_HZ before lowering gain.
# Clips-only change: regenerate + uploadfs, firmware untouched.
HPF_HZ="${HPF_HZ:-250}"          # high-pass corner, Hz (applied twice = 24 dB/oct)
PRESENCE_DB="${PRESENCE_DB:-2.5}" # presence lift at 3 kHz, dB
COMP_MAKEUP="${COMP_MAKEUP:-4}"  # compressor make-up gain, dB
# GAIN_DB=6 with THIS chain is NOT the old "clipped" 6: the HPF removed the excursion-
# burning low band and the compressor tamed the peaks first. Measured on "CW Keyer":
# >250 Hz band RMS -20.5 dB (~+2 dB louder than the gain-3 set), peak -2.6 dBFS.
# 2026-08-22: raised 6 -> 10 to win back part of what de-clipping the DAC cost (above).
# This gain drives the limiter, which is what buys loudness here: the compressor route
# was tried and is WORSE in ffmpeg (a lower threshold attenuates more than the make-up
# restores). Swept 6/8/10/12/14 dB against RMS at the DAC, limiter workload and decoder
# saturation, modelling the real chain (Helix int16 decode -> VolumeStream 0.7 -> DAC +2 dB):
#   6 -> +0.00 dB RMS, 0.14 % of samples limited     10 -> +2.05 dB, 1.67 %   <- chosen
#   8 -> +1.18 dB RMS, 0.56 %                        12 -> +2.70 dB, 3.82 %
#                                                    14 -> +3.18 dB, 7.37 % AND the MP3
#                                                          decoder saturates. Do not.
# 10 dB is the knee: 12 buys 0.65 dB more for over double the limiting. Note this only
# recovers ~2 dB of the ~9 dB that de-clipping cost -- the rest is not available in the
# digital domain (see PEAK_CEILING) and needs an analog-side decision. Loudness beyond
# this must NOT be bought by pushing GAIN_DB up; that just re-creates the old distortion.
GAIN_DB="${GAIN_DB:-10}"
# Post-encode safety ceiling on the DECODED peak, in the int16 domain the on-device Helix
# decoder actually produces. 32 kbps MP3 encoding moves peaks unpredictably (+/- 3 dB
# observed), so a clip can come out of the encoder hotter than the pre-encode limiter
# allowed and saturate the DECODER -- 2 of every 100 shipped clips did exactly that.
# Every clip is measured after encoding and re-encoded with a trim if it lands over this.
# 0.95 leaves headroom below full scale; the codec DAC cannot clip below it (the sidetone
# path scales by 0.7 and the DAC adds +2 dB, so 0.95 * 0.7 * 1.259 = 0.84).
PEAK_CEILING="${PEAK_CEILING:-0.95}"
# Silence padding. The async player hands the mixer back the instant a clip's file is read,
# leaving ~80 ms of decoded tail that plays at the START of the NEXT clip. Trailing silence
# makes that residue (and the cut) inaudible; a little leading silence hides MP3 decoder priming.
LEAD_MS="${LEAD_MS:-40}"    # leading silence, milliseconds
TRAIL_S="${TRAIL_S:-0.18}"  # trailing silence, seconds (> the ~80 ms result-queue tail)

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
command -v ffmpeg >/dev/null || { echo "ffmpeg not found" >&2; exit 1; }
mkdir -p "$OUTDIR"

# ---- Clip id: first 8 hex of md5(text). Must match extract_voice_strings.py so the
#      manifest and the on-disk /voice/<id>.mp3 names agree. SPIFFS caps the full path
#      at 32 chars, so we cannot use the (long, human-readable) UI strings as names.
clip_id() {
  if command -v md5 >/dev/null 2>&1; then printf '%s' "$1" | md5 -q | cut -c1-8
  else printf '%s' "$1" | md5sum | cut -c1-8; fi
}

generated=0 skipped=0 empty=0 trimmed=0
# PEAK_CEILING is linear (0..1); the peak check below works in dB.
CEIL_DB="$(awk -v c="$PEAK_CEILING" 'BEGIN{printf "%.2f", 20*log(c)/log(10)}')"
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
  # Encode to MP3 at the M32 I2S format (44100 Hz stereo); -ac 2 duplicates the mono voice.
  # Chain: micro-speaker EQ (high-pass + presence) -> speech compression -> gain ->
  # brick-wall limiter -> post-encode trim -> lead/trail silence (hides the async cut +
  # decoder priming). $1 = trim in dB applied after the limiter (0 on the first pass).
  encode_clip() {
    ffmpeg -y -i "$TMP/${FNAME}.wav" \
           -af "highpass=f=${HPF_HZ},highpass=f=${HPF_HZ},equalizer=f=3000:t=q:w=1.0:g=${PRESENCE_DB},acompressor=threshold=-18dB:ratio=2.5:attack=4:release=140:makeup=${COMP_MAKEUP}dB,volume=${GAIN_DB}dB,alimiter=limit=0.95,volume=${1}dB,adelay=${LEAD_MS}|${LEAD_MS},apad=pad_dur=${TRAIL_S}" \
           -ar "$OUT_SR" -ac "$OUT_CH" -b:a "${BITRATE}k" "$OUT" 2>/dev/null
  }
  # Peak of the encoded file as the DEVICE will decode it. volumedetect runs on the
  # decoder's own (unclamped, float) output, so an overshoot is reported instead of
  # being silently flattened -- which is what lets us compute the trim in one step.
  decoded_peak_db() {
    # NB: volumedetect reports at log level "info" -- with -v quiet the summary is
    # suppressed and this silently returns nothing, disabling the whole check.
    ffmpeg -hide_banner -nostats -i "$OUT" -af volumedetect -f null - 2>&1 \
      | sed -n 's/.*max_volume: \(.*\) dB/\1/p' | tail -1
  }

  # Re-encode with a trim if the encoder pushed this clip past the ceiling. MP3 decode is
  # very nearly linear in the input scale, so the first correction lands close -- but not
  # exactly (trimming the input shifts the encoder's quantisation decisions), so converge
  # over a couple of passes. Fires on only a handful of clips at the shipping GAIN_DB.
  TRIM=0; encode_clip "$TRIM"
  for pass in 1 2 3; do
    PEAK_DB="$(decoded_peak_db)"
    if [ -z "$PEAK_DB" ]; then
      echo "warning: $FNAME -- no peak reading, left untrimmed" >&2; break
    fi
    # Under the ceiling: done, whichever pass we are on.
    awk -v p="$PEAK_DB" -v c="$CEIL_DB" 'BEGIN{exit !(p>c)}' || break
    TRIM="$(awk -v t="$TRIM" -v p="$PEAK_DB" -v c="$CEIL_DB" 'BEGIN{printf "%.2f", t+(c-p)}')"
    encode_clip "$TRIM"
    if [ "$pass" -eq 1 ]; then trimmed=$((trimmed+1)); fi
    # Warn only if the LAST pass still left it over -- re-measure, don't assume.
    if [ "$pass" -eq 3 ]; then
      FINAL_DB="$(decoded_peak_db)"
      if [ -n "$FINAL_DB" ] && awk -v p="$FINAL_DB" -v c="$CEIL_DB" 'BEGIN{exit !(p>c)}'; then
        echo "warning: $FNAME still ${FINAL_DB} dBFS after ${TRIM} dB (ceiling ${CEIL_DB})" >&2
      fi
    fi
  done
  rm -f "$TMP/${FNAME}.wav"
  generated=$((generated+1))
done < "$INPUT"

# ---- Orphans ----------------------------------------------------------------
# Clip filenames are md5(text), so a clip whose string is removed from
# voice_strings.txt is never overwritten -- it just lingers, and (now that the
# clips are committed) would sit in the repo and on SPIFFS forever. Always
# report; delete only when asked.
EXPECTED="$TMP/expected.txt"
: > "$EXPECTED"
while IFS= read -r TEXT || [ -n "$TEXT" ]; do
  [ -z "$TEXT" ] && continue
  clip_id "$TEXT" >> "$EXPECTED"
done < "$INPUT"
sort -u -o "$EXPECTED" "$EXPECTED"

orphans=0 pruned=0
while IFS= read -r f; do
  [ -z "$f" ] && continue
  if ! grep -qxF "$(basename "$f" .mp3)" "$EXPECTED"; then
    orphans=$((orphans+1))
    if [ "$PRUNE" -eq 1 ]; then
      rm -f "$f"; pruned=$((pruned+1))
    else
      echo "orphan (re-run with --prune to delete): $f" >&2
    fi
  fi
done < <(find "$OUTDIR" -name '*.mp3')

# ---- Summary ----------------------------------------------------------------
total_kb=$(du -sk "$OUTDIR" | awk '{print $1}')
count=$(find "$OUTDIR" -name '*.mp3' | wc -l | tr -d ' ')
echo "----------------------------------------------"
echo "input            : $INPUT"
echo "engine           : $TTS_ENGINE${TTS_ENGINE:+ }$([ "$TTS_ENGINE" = piper ] && echo "(alan, length-scale ${LENGTH_SCALE})")"
echo "format           : ${BITRATE} kbps @ ${OUT_SR} Hz, ${OUT_CH}ch (matches M32 I2S)"
echo "loudness         : gain ${GAIN_DB} dB, HPF ${HPF_HZ} Hz, decoded-peak ceiling ${PEAK_CEILING} (${CEIL_DB} dBFS)"
echo "generated now    : $generated"
echo "peak-trimmed     : $trimmed  (encoder overshoot pulled back under the ceiling)"
echo "skipped (exist)  : $skipped"
echo "empty-slug skips : $empty"
orphan_note=""
if [ "$PRUNE" -eq 1 ]; then orphan_note=" (deleted $pruned)"
elif [ "$orphans" -gt 0 ]; then orphan_note=" (kept -- re-run with --prune)"; fi
echo "orphans          : ${orphans}${orphan_note}"
echo "clips in $OUTDIR/ : $count"
echo "total size       : ${total_kb} KB"
