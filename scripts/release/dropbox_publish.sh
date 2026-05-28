#!/usr/bin/env bash
# dropbox_publish.sh — copy renamed firmware binaries into the Dropbox
# folder backing the web installer, and prepend entries to each platform's
# versions.json manifest.
#
# Reads MORSERINO_DROPBOX_ROOT from the environment (set on the runner —
# see RUNNER_SETUP.md §3).
#
# Inputs (all required except where noted):
#   --rename-info PATH    JSON file emitted by rename_binaries.py
#   --channel stable|beta
#   --notes "TEXT"        optional; overrides the templated default in
#                         update_manifest.py
#   --dry-run             pass-through: copy → echo, manifest update → dry-run
#
# Exit non-zero on any error. set -euo pipefail enforces fail-fast.

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

usage() {
    sed -n '2,15p' "$0"
    exit "${1:-2}"
}

RENAME_INFO=""
CHANNEL=""
NOTES=""
NOTES_SET=0
DRY_RUN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --rename-info) RENAME_INFO="$2"; shift 2 ;;
        --channel)     CHANNEL="$2";     shift 2 ;;
        --notes)       NOTES="$2"; NOTES_SET=1; shift 2 ;;
        --dry-run)     DRY_RUN=1; shift ;;
        -h|--help)     usage 0 ;;
        *) echo "Unknown argument: $1" >&2; usage 2 ;;
    esac
done

[[ -n "$RENAME_INFO" ]]  || { echo "--rename-info is required" >&2; exit 2; }
[[ -n "$CHANNEL"     ]]  || { echo "--channel is required" >&2; exit 2; }
[[ -f "$RENAME_INFO" ]]  || { echo "Rename info not found: $RENAME_INFO" >&2; exit 1; }

if [[ "$CHANNEL" != "stable" && "$CHANNEL" != "beta" ]]; then
    echo "--channel must be 'stable' or 'beta', got: $CHANNEL" >&2
    exit 2
fi

if [[ -z "${MORSERINO_DROPBOX_ROOT:-}" ]]; then
    echo "MORSERINO_DROPBOX_ROOT is not set. See RUNNER_SETUP.md §3." >&2
    exit 1
fi
if [[ ! -d "$MORSERINO_DROPBOX_ROOT" ]]; then
    echo "MORSERINO_DROPBOX_ROOT does not exist or is not a directory:" >&2
    echo "  $MORSERINO_DROPBOX_ROOT" >&2
    exit 1
fi

log() {
    local prefix=""
    [[ $DRY_RUN -eq 1 ]] && prefix="[dry-run] "
    echo "${prefix}$*" >&2
}

# Iterate platforms from the JSON. We need: key, filename, staged_path,
# manifest_version, dropbox_subdir.
PLATFORM_KEYS=$(python3 -c "import json,sys; print('\n'.join(json.load(open(sys.argv[1])).keys()))" "$RENAME_INFO")

for key in $PLATFORM_KEYS; do
    log "--- platform: $key ---"

    filename=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['$key']['filename'])" "$RENAME_INFO")
    staged=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['$key']['staged_path'])" "$RENAME_INFO")
    mver=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['$key']['manifest_version'])" "$RENAME_INFO")
    subdir=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['$key']['dropbox_subdir'])" "$RENAME_INFO")

    if [[ -n "$subdir" ]]; then
        target_dir="$MORSERINO_DROPBOX_ROOT/$subdir"
    else
        target_dir="$MORSERINO_DROPBOX_ROOT"
    fi
    target_file="$target_dir/$filename"
    target_manifest="$target_dir/versions.json"

    if [[ ! -d "$target_dir" ]]; then
        echo "Target directory does not exist: $target_dir" >&2
        exit 1
    fi
    if [[ ! -f "$staged" ]]; then
        echo "Staged binary not found: $staged" >&2
        exit 1
    fi

    log "copy $staged → $target_file"
    if [[ $DRY_RUN -eq 0 ]]; then
        cp "$staged" "$target_file"
    fi

    # Manifest update. Pass --notes only if it was explicitly set; otherwise
    # let update_manifest.py compute the templated default.
    UPDATE_ARGS=(
        --manifest "$target_manifest"
        --version  "$mver"
        --filename "$filename"
        --channel  "$CHANNEL"
    )
    if [[ $NOTES_SET -eq 1 ]]; then
        UPDATE_ARGS+=(--notes "$NOTES")
    fi
    if [[ $DRY_RUN -eq 1 ]]; then
        UPDATE_ARGS+=(--dry-run)
    fi

    log "update_manifest.py ${UPDATE_ARGS[*]}"
    python3 "$SCRIPT_DIR/update_manifest.py" "${UPDATE_ARGS[@]}" >/dev/null
done

log "done."
