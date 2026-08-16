#!/bin/bash
#
# Scaffold the User Manual directory for a new major firmware version.
#
# The release workflow (.github/workflows/release.yml) derives <major> from the
# git tag and then requires:
#
#     Documentation/User Manual/Version <major>.x/build.sh
#         -> m32UserManual_v<major>_en.pdf
#         -> m32UserManual_v<major>_de.pdf
#
# So the moment master's VERSION_MAJOR is bumped, that folder has to exist with
# the right name, or tagging fails late and confusingly. This script creates it
# from the previous major version's folder and rewrites the version strings that
# are easy to miss by hand (the title pages, the "this manual reflects firmware
# version N.x" sentence, and the update_m32 example filenames).
#
# Usage:
#   ./new-major-version.sh            # target = VERSION_MAJOR from morsedefs.h
#   ./new-major-version.sh 10         # target = 10, source = highest below it
#   ./new-major-version.sh 10 --from 9
#   ./new-major-version.sh --check    # verify only, change nothing (used by CI)
#
# After running it, the remaining work is editorial and yours: go through
# manual_en.md / manual_de.md and document what actually changed in the new
# version.

set -u

cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1
MANUAL_ROOT="$PWD"
MORSEDEFS="../../Software/src/Version 6 and newer/morsedefs.h"

CHECK_ONLY=0
TARGET=""
SOURCE=""

# In-place sed differs between BSD (macOS, where this is normally run) and GNU
# (the CI runner, which only ever uses --check, but keep it portable anyway).
if sed --version >/dev/null 2>&1; then SEDI=(sed -i); else SEDI=(sed -i ''); fi

while [ $# -gt 0 ]; do
    case "$1" in
        --check)  CHECK_ONLY=1; shift ;;
        --from)   SOURCE="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        [0-9]*)   TARGET="$1"; shift ;;
        *)        echo "Unknown argument: $1"; exit 1 ;;
    esac
done

# ---------------------------------------------------------------- firmware
firmware_major() {
    sed -n 's/^#define VERSION_MAJOR \([0-9][0-9]*\).*/\1/p' "$MORSEDEFS" | head -1
}

FW_MAJOR=$(firmware_major)
if [ -z "$FW_MAJOR" ]; then
    echo "ERROR: could not read VERSION_MAJOR from $MORSEDEFS"
    exit 1
fi

[ -n "$TARGET" ] || TARGET="$FW_MAJOR"

# ------------------------------------------------------------------- check
# The invariant: the firmware's major version has a manual folder, and that
# folder's build.sh produces PDFs named for that same major version.
if [ "$CHECK_ONLY" = "1" ]; then
    dir="Version ${FW_MAJOR}.x"
    fail=0

    if [ ! -d "$dir" ]; then
        echo "FAIL: firmware is at V${FW_MAJOR} but '$dir' does not exist."
        echo "      Run: Documentation/User Manual/new-major-version.sh"
        fail=1
    elif [ ! -x "$dir/build.sh" ]; then
        echo "FAIL: $dir/build.sh is missing or not executable."
        fail=1
    else
        # build.sh must not hardcode a different version than its own folder.
        if grep -q "m32UserManual_v${FW_MAJOR}_\|m32UserManual_v\${MAJOR}_" "$dir/build.sh"; then
            :
        else
            echo "FAIL: $dir/build.sh does not produce m32UserManual_v${FW_MAJOR}_*.pdf."
            echo "      It probably still carries the previous version's filenames."
            fail=1
        fi
        for f in manual_en.md manual_de.md title.html title_de.html style.css normalize_ids.lua variant.lua epub.css; do
            [ -f "$dir/$f" ] || { echo "FAIL: $dir/$f is missing."; fail=1; }
        done
        # Only the places that MUST carry the current version are checked by
        # name. A blanket "no other N.x anywhere" scan would trip over the
        # legitimate historical references ("if you are still on version 1.x").
        check_anchor() {                       # file, sed-extract, description
            local f="$dir/$1" expr="$2" label="$3" found n
            [ -f "$f" ] || return 0
            found=$(sed -n "$expr" "$f" | sort -u)
            if [ -z "$found" ]; then
                echo "WARN: '$label' not found in $1 — template changed, coverage lost."
                return 0
            fi
            for n in $found; do
                if [ "$n" != "$FW_MAJOR" ]; then
                    echo "FAIL: $1 — $label says ${n}.x, but the firmware is V${FW_MAJOR}."
                    return 1
                fi
            done
        }

        # The title pages are templates now: build.sh fills @MAJOR@ from the
        # folder name and @MONTH_YEAR@ / @YEAR@ from the build date, so their
        # version can no longer drift and there is no value to compare. What
        # CAN go wrong is someone "fixing" a template by typing a literal
        # version back in, which would freeze it again — so check the opposite:
        # that the placeholders are still there and no literal N.x crept in.
        check_template() {                     # file, description
            local f="$dir/$1" label="$2" ph rc=0
            [ -f "$f" ] || return 0
            for ph in '@MAJOR@' '@MONTH_YEAR@' '@VARIANT@'; do
                if ! grep -q -- "$ph" "$f"; then
                    echo "FAIL: $1 — $label: placeholder $ph is gone."
                    echo "      build.sh fills it in; a literal value here goes stale silently."
                    rc=1
                fi
            done
            if grep -q 'title-version">Version [0-9]' "$f"; then
                echo "FAIL: $1 — $label: literal version on the title page instead of @MAJOR@."
                rc=1
            fi
            return $rc
        }
        check_template title.html    'title page' || fail=1
        check_template title_de.html 'Titelseite' || fail=1
        check_anchor manual_en.md  's/.*firmware Version \([0-9][0-9]*\)\.x.*/\1/p'                'intro paragraph'         || fail=1
        check_anchor manual_de.md  's/.*Firmware-Version \([0-9][0-9]*\)\.x des.*/\1/p'            'Einleitungsabsatz'       || fail=1
        check_anchor manual_en.md  's/.*m32_V\([0-9][0-9]*\)\.[0-9]*\.bin.*/\1/p'                  'update_m32 examples'     || fail=1
        check_anchor manual_de.md  's/.*m32_V\([0-9][0-9]*\)\.[0-9]*\.bin.*/\1/p'                  'update_m32 Beispiele'    || fail=1
    fi

    [ "$fail" = "0" ] && echo "OK: user manual folder matches firmware V${FW_MAJOR}."
    exit $fail
fi

# ------------------------------------------------------------------ create
TARGET_DIR="Version ${TARGET}.x"

if [ -d "$TARGET_DIR" ]; then
    echo "ERROR: '$TARGET_DIR' already exists — refusing to overwrite."
    echo "       Delete it first if you really want to start over."
    exit 1
fi

if [ -z "$SOURCE" ]; then
    # highest existing Version N.x below the target
    SOURCE=$(ls -d Version\ [0-9]*.x 2>/dev/null \
             | sed -n 's/^Version \([0-9][0-9]*\)\.x$/\1/p' \
             | sort -n | awk -v t="$TARGET" '$1 < t' | tail -1)
fi
SOURCE_DIR="Version ${SOURCE}.x"

if [ -z "$SOURCE" ] || [ ! -d "$SOURCE_DIR" ]; then
    echo "ERROR: no source manual directory found (looked for '$SOURCE_DIR')."
    exit 1
fi

echo "Creating '$TARGET_DIR' from '$SOURCE_DIR'"
mkdir -p "$TARGET_DIR"

# Copy sources only. The .html and .pdf are build products — carrying them over
# is how "m32UserManual_v8_en.pdf" ended up inside Version 9.x. Finder-style
# " 2." duplicates are excluded for the same reason.
for f in manual_en.md manual_de.md title.html title_de.html style.css normalize_ids.lua variant.lua epub.css build.sh; do
    if [ -f "$SOURCE_DIR/$f" ]; then
        cp "$SOURCE_DIR/$f" "$TARGET_DIR/$f"
    else
        echo "  WARNING: $SOURCE_DIR/$f not found, skipped"
    fi
done
[ -d "$SOURCE_DIR/images" ] && cp -R "$SOURCE_DIR/images" "$TARGET_DIR/images"
chmod +x "$TARGET_DIR/build.sh"

# Any stray Finder duplicates that came along anyway.
find "$TARGET_DIR" -name "* [0-9].*" -delete

# ----------------------------------------------------------------- rewrite
# The title pages need no rewriting: they are templates, and build.sh resolves
# @MAJOR@ from the folder name and @MONTH_YEAR@ / @YEAR@ from the build date.
# (Before that they were sed-rewritten here, which fixed the version but left
# the month frozen at whatever day this script happened to be run.)

cd "$TARGET_DIR" || exit 1

"${SEDI[@]}" \
    -e "s|firmware Version ${SOURCE}\.x|firmware Version ${TARGET}.x|g" \
    -e "s|m32_V${SOURCE}\.0\.bin|m32_V${TARGET}.0.bin|g" \
    manual_en.md

"${SEDI[@]}" \
    -e "s|Firmware-Version ${SOURCE}\.x|Firmware-Version ${TARGET}.x|g" \
    -e "s|m32_V${SOURCE}\.0\.bin|m32_V${TARGET}.0.bin|g" \
    manual_de.md

# build.sh derives its version from the folder name, so it needs no edit — but
# older copies hardcoded it. Fix those up rather than shipping a wrong filename.
if grep -q "m32UserManual_v${SOURCE}_" build.sh; then
    echo "  NOTE: source build.sh hardcoded v${SOURCE}; rewriting to v${TARGET}."
    "${SEDI[@]}" "s|m32UserManual_v${SOURCE}_|m32UserManual_v${TARGET}_|g" build.sh
fi

cd "$MANUAL_ROOT" || exit 1

echo ""
echo "=== Created '$TARGET_DIR' ==="
echo ""
if [ "$TARGET" != "$FW_MAJOR" ]; then
    echo "  WARNING: morsedefs.h VERSION_MAJOR is $FW_MAJOR, not $TARGET."
    echo "           Bump the firmware version too, or the release will not match."
    echo ""
fi
echo "Remaining, by hand:"
echo "  1. Refresh the 'What is new' section:"
echo "       Documentation/User Manual/sync_whatsnew.py --write"
echo "     then translate the English block into manual_de.md (see --check)."
echo "  2. Document anything else that changed in manual_en.md / manual_de.md."
echo "  3. Build and eyeball:  (cd '$TARGET_DIR' && ./build.sh)"
echo "  4. git add '$TARGET_DIR' && commit."
echo ""
echo "The title pages need no edit: version and month are filled in at build time."
echo ""
echo "Any leftover reference to the old version:"
grep -rn "${SOURCE}\.x\|m32_V${SOURCE}\." "$TARGET_DIR"/title*.html "$TARGET_DIR"/manual_*.md 2>/dev/null \
    | grep -v "Version [1-9]*\.x of the manual" | head -20
echo "(nothing listed above means the rewrite was complete)"
