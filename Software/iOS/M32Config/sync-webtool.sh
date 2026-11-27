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

# The app makes no network requests (a promise in the App Review notes). In a
# browser both pages load the Lato font from Google Fonts and the Config Tool
# loads pdf.js, mammoth and JSZip from cdnjs; the app carries its own copies in
# Resources/Web/vendor (checked in, licences in vendor/THIRD_PARTY_LICENSES.txt),
# and the COPIES are pointed at them here. The pages in Software/Utilities stay
# as they are. WebToolView also blocks any remote load as a safety net.
cdn=https://cdnjs.cloudflare.com/ajax/libs
fonts="https://fonts.googleapis.com/css2?family=Lato:ital,wght@0,400;0,700;0,900;1,400;1,700\&display=swap"
for file in m32_config_tool.html m32_trainer.html; do
    sed -i '' \
        -e "s|$cdn/pdf.js/3.11.174/pdf.min.js|vendor/pdf.min.js|g" \
        -e "s|$cdn/pdf.js/3.11.174/pdf.worker.min.js|vendor/pdf.worker.min.js|g" \
        -e "s|$cdn/mammoth/1.6.0/mammoth.browser.min.js|vendor/mammoth.browser.min.js|g" \
        -e "s|$cdn/jszip/3.10.1/jszip.min.js|vendor/jszip.min.js|g" \
        -e "s|$fonts|vendor/lato.css|g" \
        "$target/$file"
done

# A page that now loads something else from the web (a new library, a version
# bump) must not slip through: fail the build and say what to bundle.
remote=$(grep -n -o -E "(src=|@import url\(|workerSrc *=) *['\"]https?://[^'\"]*" \
         "$target/m32_config_tool.html" "$target/m32_trainer.html" || true)
if [ -n "$remote" ]; then
    echo "sync-webtool.sh: a page loads from the web; bundle it in Resources/Web/vendor and map it here:" >&2
    echo "$remote" >&2
    exit 1
fi

echo "Web pages synced into Resources/Web (bridge.js and trainer-bridge.js are checked in and stay put)."
