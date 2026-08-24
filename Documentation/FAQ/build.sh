#!/bin/bash
#
# Build the Morserino-32 Pocket FAQ as PDF, from the Markdown sources.
#
# Requirements:
#   - pandoc
#   - weasyprint (pip install weasyprint)
#   - Lato font installed (falls back to Helvetica/Arial without it)
#
# Usage:
#   ./build.sh            # both languages
#   ./build.sh en         # English only
#   ./build.sh de         # German only
#   ./build.sh en html    # keep the intermediate HTML instead, for inspection
#
# The English PDF keeps its established file name, so links to it stay valid.
#
# Like the protocol document, this used to be a hand export and went stale. Run
# this after every change to either Markdown file - and to both languages, so the
# two do not drift apart.

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

for tool in pandoc weasyprint; do
    command -v "$tool" >/dev/null 2>&1 || { echo "ERROR: $tool is not installed." >&2; exit 1; }
done

LANGS=${1:-all}
FORMAT=${2:-pdf}

build_one() {
    local lang=$1 src title toc html pdf
    case "$lang" in
        en) src="Morserino-32 Pocket FAQ.md"
            pdf="Morserino-32 Pocket FAQ.pdf"
            title="Morserino-32 Pocket FAQ"
            toc="Contents" ;;
        de) src="Morserino-32 Pocket FAQ (Deutsch).md"
            pdf="Morserino-32 Pocket FAQ (Deutsch).pdf"
            title="Morserino-32 Pocket FAQ (Deutsch)"
            toc="Inhalt" ;;
        *)  echo "ERROR: unknown language '$lang' (use en or de)." >&2; return 1 ;;
    esac
    html="${pdf%.pdf}.html"

    [ -f "$src" ] || { echo "ERROR: $src is missing." >&2; return 1; }

    # -raw_html for the same reason as the protocol document: the sources talk
    # about file names and settings in running prose, and pandoc's default reader
    # would swallow anything shaped like a tag. There is no intentional HTML here.
    pandoc -f markdown-raw_html -t html5 \
           --standalone \
           --toc --toc-depth=2 \
           --metadata pagetitle="$title" \
           --metadata toc-title="$toc" \
           --metadata lang="$lang" \
           --css style.css \
           -o "$html" "$src"

    if [ "$FORMAT" = "html" ]; then
        echo "Wrote $html"
        return 0
    fi

    weasyprint "$html" "$pdf"
    rm -f "$html"
    echo "Wrote $pdf"
}

case "$LANGS" in
    all) build_one en && build_one de ;;
    *)   build_one "$LANGS" ;;
esac
