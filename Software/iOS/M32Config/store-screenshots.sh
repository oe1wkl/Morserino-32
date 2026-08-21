#!/bin/sh
#
# Convert iPhone screenshots to the size App Store Connect demands.
#
#   ./store-screenshots.sh ~/Desktop/shots            # writes ~/Desktop/shots/appstore/
#   ./store-screenshots.sh ~/Desktop/shots /tmp/out   # or name the output directory
#
# Apple's required slot for an iPhone-only app is the 6.9-inch display,
# 1320x2868. Screenshots off a 14 Pro are 1179x2556 (6.1-inch) and are refused
# at upload, so they have to be resized.
#
# The two aspect ratios are not quite identical -- 0.46127 against 0.46025 --
# so this stretches by about 0.2%, which is invisible. The alternative, padding
# to the taller canvas, leaves bars that are far more noticeable than a
# fifth of a percent of stretch.
#
# Anything already 1320x2868 is copied through untouched, so it is safe to run
# over a folder that mixes devices.

set -eu

SRC=${1:-}
[ -n "$SRC" ] && [ -d "$SRC" ] || { echo "usage: $0 <folder-of-screenshots> [output-folder]" >&2; exit 1; }
OUT=${2:-$SRC/appstore}
W=1320
H=2868

mkdir -p "$OUT"
n=0
for f in "$SRC"/*.png "$SRC"/*.PNG "$SRC"/*.jpg "$SRC"/*.JPG; do
    [ -e "$f" ] || continue
    base=$(basename "$f")
    base="${base%.*}.png"
    w=$(sips -g pixelWidth  "$f" | awk '/pixelWidth/{print $2}')
    h=$(sips -g pixelHeight "$f" | awk '/pixelHeight/{print $2}')
    if [ "$w" = "$W" ] && [ "$h" = "$H" ]; then
        cp "$f" "$OUT/$base"
        printf '  %-40s %sx%s  already correct\n' "$base" "$w" "$h"
    else
        sips -s format png -z "$H" "$W" "$f" --out "$OUT/$base" >/dev/null
        printf '  %-40s %sx%s -> %sx%s\n' "$base" "$w" "$h" "$W" "$H"
    fi
    n=$((n+1))
done

[ "$n" -gt 0 ] || { echo "no images found in $SRC" >&2; exit 1; }
echo
echo "$n image(s) in $OUT, ready to drag into App Store Connect."
