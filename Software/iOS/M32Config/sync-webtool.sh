#!/bin/sh
#
# Copy the web config tool and the M32 Trainer into the app bundle's Web/ directory.
#
# Neither is forked for iOS. Software/Utilities stays the single source of truth
# for both the browser and this app; the copies under Resources/Web are build
# inputs and are git-ignored. Re-run this after every change to either page.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
utilities="$here/../../Utilities"
target="$here/Resources/Web"

mkdir -p "$target"

for file in m32_config_tool.html m32_pref_help.json \
            m32_trainer.html m32_trainer_link.js m32_trainer_engine.js m32_trainer_corpus.json; do
    if [ ! -f "$utilities/$file" ]; then
        echo "sync-webtool.sh: missing $utilities/$file" >&2
        exit 1
    fi
    cp "$utilities/$file" "$target/$file"
    echo "  $file"
done

echo "Web pages synced into Resources/Web (bridge.js and trainer-bridge.js are checked in and stay put)."
