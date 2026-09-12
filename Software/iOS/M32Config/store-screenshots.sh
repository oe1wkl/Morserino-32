#!/bin/sh
#
# Convert iPhone screenshots to the sizes App Store Connect accepts.
#
#   ./store-screenshots.sh ~/Desktop/shots            # writes ~/Desktop/shots/appstore/
#   ./store-screenshots.sh ~/Desktop/shots /tmp/out   # or name the output directory
#
# App Store Connect has a DEVICE SIZE SELECTOR above the upload area, and each
# slot accepts only its own dimensions. UPLOAD TO THE 6.9-INCH TAB -- that is
# the one that worked (09/2026). If you land on the 6.5-inch tab instead it
# rejects 1320x2868 and lists 1242x2688 / 1284x2778 as "required", which reads
# like the files are wrong when in fact the tab is.
#
# Both sizes are produced so either tab can be fed:
#
#   6.9-inch/  1320x2868   iPhone 16/17 Pro Max class
#   6.5-inch/  1284x2778   the size App Store Connect asked for in 09/2026
#                          (it also accepts 1242x2688 in that same slot)
#
# Screenshots off a 14 Pro are 1179x2556 and fit neither, so they are refused at
# upload until converted. Every one of these aspect ratios is within about 0.2%
# of the others (0.4613 / 0.4603 / 0.4622), so this stretches rather than pads:
# a fifth of a percent is invisible, letterbox bars are not.
#
# An image already at a target size is copied through untouched, so it is safe
# to re-run over a folder that already holds converted shots.

set -eu

SRC=${1:-}
[ -n "$SRC" ] && [ -d "$SRC" ] || { echo "usage: $0 <folder-of-screenshots> [output-folder]" >&2; exit 1; }
OUT=${2:-$SRC/appstore}

# "<width>x<height>:<folder name>"
SIZES="1320x2868:6.9-inch 1284x2778:6.5-inch"

total=0
for spec in $SIZES; do
    dims=${spec%%:*}
    name=${spec##*:}
    W=${dims%%x*}
    H=${dims##*x}
    mkdir -p "$OUT/$name"
    n=0
    for f in "$SRC"/*.png "$SRC"/*.PNG "$SRC"/*.jpg "$SRC"/*.JPG; do
        [ -e "$f" ] || continue
        base=$(basename "$f"); base="${base%.*}.png"
        w=$(sips -g pixelWidth  "$f" | awk '/pixelWidth/{print $2}')
        h=$(sips -g pixelHeight "$f" | awk '/pixelHeight/{print $2}')
        if [ "$w" = "$W" ] && [ "$h" = "$H" ]; then
            cp "$f" "$OUT/$name/$base"
        else
            sips -s format png -z "$H" "$W" "$f" --out "$OUT/$name/$base" >/dev/null
        fi
        n=$((n+1))
    done
    [ "$n" -gt 0 ] || { echo "no images found in $SRC" >&2; exit 1; }
    echo "  $name: $n image(s) at ${W}x${H}"
    total=$n
done

echo
echo "$total screenshot(s), in both sizes, under $OUT"
echo "Upload the folder that matches the size App Store Connect is asking for."
