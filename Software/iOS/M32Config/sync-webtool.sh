#!/bin/sh
#
# Copy the web config tool into the app bundle's Web/ directory.
#
# The tool is NOT forked for iOS. Software/Utilities/m32_config_tool.html stays
# the single source of truth for both the browser and this app; the copies under
# Resources/Web are build inputs and are git-ignored. Re-run this after every
# change to the tool.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
utilities="$here/../../Utilities"
target="$here/Resources/Web"

mkdir -p "$target"

for file in m32_config_tool.html m32_pref_help.json; do
    if [ ! -f "$utilities/$file" ]; then
        echo "sync-webtool.sh: missing $utilities/$file" >&2
        exit 1
    fi
    cp "$utilities/$file" "$target/$file"
    echo "  $file"
done

echo "Web tool synced into Resources/Web (bridge.js is checked in and stays put)."
