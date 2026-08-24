#!/bin/bash
#
# Build "M32 Protocol.pdf" from "M32 Protocol.md".
#
# Requirements:
#   - pandoc
#   - weasyprint (pip install weasyprint)
#   - Lato font installed (falls back to Helvetica/Arial without it)
#
# Usage:
#   ./build.sh          # write M32 Protocol.pdf
#   ./build.sh html     # keep the intermediate HTML instead, for inspection
#
# Why this script exists: the PDF used to be exported by hand from MacDown. That
# export was made in April 2026 and then sat unchanged while the Markdown gained
# protocol 1.3 content, the Transports (BLE Serial) section, the device "edition"
# property and the file-listing fix - four revisions of drift in a document that
# is the more likely one for a non-developer to open. Regenerate with this after
# every change to the Markdown.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

SRC="M32 Protocol.md"
HTML="M32 Protocol.html"
PDF="M32 Protocol.pdf"

for tool in pandoc weasyprint; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "ERROR: $tool is not installed." >&2
        exit 1
    }
done

# -raw_html is essential here, not cosmetic. This document describes commands
# using placeholders in running prose - <value>, <n>, <callsign>, <characters> -
# and refers to the file player's <p> pause marker. Pandoc's default markdown
# reader parses every one of those as an inline raw HTML tag, which then
# disappears from the rendered output: "set the keyer speed to <value>" silently
# becomes "set the keyer speed to". The document contains no intentional HTML, so
# turning the extension off makes all of them literal text.
#
# pagetitle (rather than title) sets the HTML <title> without also emitting a
# title block, which would duplicate the document's own top-level heading.
pandoc -f markdown-raw_html -t html5 \
       --standalone \
       --toc --toc-depth=3 \
       --metadata pagetitle="The M32 Serial Protocol" \
       --css style.css \
       -o "$HTML" "$SRC"

if [ "${1:-pdf}" = "html" ]; then
    echo "Wrote $HTML"
    exit 0
fi

weasyprint "$HTML" "$PDF"
rm -f "$HTML"
echo "Wrote $PDF"
