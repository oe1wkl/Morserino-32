#!/bin/bash
# Build a LOCAL firmware/ tree for the M32 web installer, so m32_installer.html can be
# exercised end-to-end without publishing anything to morserino.info.
#
# Mirrors the server layout that targets.json describes: every "dir" and "versions" path
# is relative to firmware/targets.json's own location, and each target's parts land under
# <dir>common/. See devdocs/installer/PLAN.md section 5.
#
# Re-run after rebuilding the firmware. Needs the three envs built AND the a11y
# filesystem image. The buildfs step must come AFTER a full a11y run - PlatformIO
# only builds the fs image on top of an existing app build for that env.
#
#   cd $REPO/Software/src && pio run -e heltec_wifi_lora_32_V2 -e pocketwroom \
#                                   -e pocketwroom-accessibility
#   cd $REPO/Software/src && pio run -e pocketwroom-accessibility -t buildfs
set -euo pipefail

# The script lives at devdocs/installer/make_installer_rig.sh, so climb two levels for
# the repo root. Overridable, but the normal case just works from wherever it is run.
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="${REPO:-$(cd "$HERE/../.." && pwd)}"
# Rig lives OUTSIDE the repo by default: the tree contains ~14 MB of binaries and a
# symlink to the served installer page, and none of that belongs in git.
RIG="${RIG:-/tmp/m32-installer-rig}"
BUILD="$REPO/Software/src/.pio/build"
UTIL="$REPO/Software/Utilities"
BOOT_APP0="$HOME/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"

# Deliberately not "9.0": the done screen says "V<version> is installed", and a rig
# install must never be mistakable for a real one.
VER="9.0b-TESTRIG"

[ -f "$BOOT_APP0" ] || { echo "boot_app0.bin not found at $BOOT_APP0" >&2; exit 1; }

# Fail early with a useful message rather than deep inside cp: an env whose build tree is
# gone (a first run, or a wipe by PLATFORMIO_DATA_DIR flipping between runs) needs a
# rebuild before the rig can be assembled.
missing=""
for env in heltec_wifi_lora_32_V2 pocketwroom pocketwroom-accessibility; do
  [ -f "$BUILD/$env/firmware.bin" ] || missing="$missing $env"
done
[ -f "$BUILD/pocketwroom-accessibility/spiffs.bin" ] || missing="$missing pocketwroom-accessibility(spiffs)"
if [ -n "$missing" ]; then
  echo "Missing build artifacts for:$missing" >&2
  echo "Run this first, from $REPO/Software/src:" >&2
  echo "  pio run -e heltec_wifi_lora_32_V2 -e pocketwroom -e pocketwroom-accessibility" >&2
  echo "  pio run -e pocketwroom-accessibility -t buildfs" >&2
  exit 1
fi

rm -rf "$RIG"
mkdir -p "$RIG/firmware/common" "$RIG/firmware/m32p/common" "$RIG/firmware/m32p-a11y/common"

# The page itself and the registry are symlinked, so the rig always reflects the repo.
ln -s "$UTIL/m32_installer.html" "$RIG/m32_installer.html"
ln -s "$UTIL/targets.json"       "$RIG/firmware/targets.json"

place() {   # place <env> <dest-dir> <app-filename>
  local env="$1" dest="$2" app="$3"
  for f in bootloader.bin partitions.bin; do
    cp "$BUILD/$env/$f" "$dest/common/$f"
  done
  cp "$BOOT_APP0" "$dest/common/boot_app0.bin"
  cp "$BUILD/$env/firmware.bin" "$dest/$app"
}

place heltec_wifi_lora_32_V2   "$RIG/firmware"           "m32_$VER.bin"
place pocketwroom              "$RIG/firmware/m32p"      "m32p_$VER.bin"
place pocketwroom-accessibility "$RIG/firmware/m32p-a11y" "m32p-a11y_$VER.bin"
cp "$BUILD/pocketwroom-accessibility/spiffs.bin" "$RIG/firmware/m32p-a11y/m32p-a11y_voice_$VER.bin"

# versions.json is a plain JSON ARRAY (loadRegistry: Array.isArray(list)); the page reads
# version / filename / fs / beta / notes off each entry.
note="LOCAL TEST RIG - built from the working tree, not a release."
cat > "$RIG/firmware/versions.json" <<JSON
[ { "version": "$VER", "filename": "m32_$VER.bin", "beta": true, "notes": "$note" } ]
JSON
cat > "$RIG/firmware/m32p/versions.json" <<JSON
[ { "version": "$VER", "filename": "m32p_$VER.bin", "beta": true, "notes": "$note" } ]
JSON
cat > "$RIG/firmware/m32p-a11y/versions.json" <<JSON
[ { "version": "$VER", "filename": "m32p-a11y_$VER.bin",
    "fs": "m32p-a11y_voice_$VER.bin", "beta": true, "notes": "$note" } ]
JSON

echo "rig at $RIG"
find "$RIG" -type f -o -type l | sed "s|$RIG|.|" | sort
echo
echo "serve with:  (cd $RIG && python3 -m http.server 8791)"
echo "then open:   http://127.0.0.1:8791/m32_installer.html"
