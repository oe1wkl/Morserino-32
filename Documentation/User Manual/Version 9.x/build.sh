#!/bin/bash
#
# Build script for Morserino-32 User Manual
# Generates PDF via HTML + weasyprint
#
# Requirements:
#   - pandoc
#   - weasyprint (pip install weasyprint)
#   - Lato font installed
#
# Usage:
#   ./build.sh              # build both PDFs (default)
#   ./build.sh en           # build English PDF
#   ./build.sh de           # build German PDF
#   ./build.sh all          # build both PDFs
#   ./build.sh en html      # build English HTML only
#   ./build.sh de html      # build German HTML only
#

# Always operate on the directory this script lives in, and take the major
# version from that directory's name ("Version 9.x" -> "9"). The release
# workflow expects m32UserManual_v<major>_{en,de}.pdf, with <major> taken from
# the tag; deriving it here instead of hardcoding it means a new major version
# only needs the folder copied, never this script edited.
cd "$(dirname "${BASH_SOURCE[0]}")" || exit 1

MAJOR=$(basename "$PWD" | sed -n 's/^Version \([0-9][0-9]*\)\.x$/\1/p')
if [ -z "$MAJOR" ]; then
    echo "ERROR: cannot derive the major version from the directory name '$(basename "$PWD")'."
    echo "       Expected a directory named 'Version <N>.x'."
    exit 1
fi

STYLE="style.css"

# Month and year of the build. The title pages carry "Version <major>.x" and a
# month, and both used to be typed by hand -- the version was rewritten by
# new-major-version.sh when the folder was created, and the month simply went
# stale from then on. Both are now placeholders in title*.html, resolved here,
# so a rebuild always dates itself correctly and no one has to remember.
YEAR=$(date "+%Y")
MONTH_EN=$(LC_ALL=C date "+%B")
case "$MONTH_EN" in
    January) MONTH_DE="Jänner" ;;      February) MONTH_DE="Februar" ;;
    March)   MONTH_DE="März"   ;;      April)    MONTH_DE="April"   ;;
    May)     MONTH_DE="Mai"    ;;      June)     MONTH_DE="Juni"    ;;
    July)    MONTH_DE="Juli"   ;;      August)   MONTH_DE="August"  ;;
    September) MONTH_DE="September" ;; October)  MONTH_DE="Oktober" ;;
    November)  MONTH_DE="November"  ;; December) MONTH_DE="Dezember" ;;
    *)         MONTH_DE="$MONTH_EN" ;;
esac

# Fill the title-page template for one language; prints the temp file's path.
resolve_title() {
    local template=$1 month=$2 out
    out=$(mktemp "${TMPDIR:-/tmp}/m32title.XXXXXX.html") || return 1
    # The BUILD-TEMPLATE comment explains the placeholders to whoever edits the
    # template; it is not for readers, so it never reaches the published HTML.
    sed -e '/<!-- BUILD-TEMPLATE/,/-->/d' \
        -e "s|@MAJOR@|${MAJOR}|g" \
        -e "s|@MONTH_YEAR@|${month} ${YEAR}|g" \
        -e "s|@YEAR@|${YEAR}|g" \
        "$template" > "$out" || { rm -f "$out"; return 1; }
    if grep -q "@[A-Z_]*@" "$out"; then
        echo "ERROR: unresolved placeholder left in $template:" >&2
        grep -o "@[A-Z_]*@" "$out" | sort -u >&2
        rm -f "$out"
        return 1
    fi
    printf '%s' "$out"
}

build_pdf() {
    local lang=$1
    local format=${2:-pdf}
    local input="manual_${lang}.md"
    local html_output="manual_${lang}.html"
    local pdf_output="m32UserManual_v${MAJOR}_${lang}.pdf"

    # Language-specific settings
    if [ "$lang" = "de" ]; then
        local title_template="title_de.html"
        local month="$MONTH_DE"
        local meta_title="Morserino-32 Benutzerhandbuch"
        local meta_lang="de"
        # Lua filter to replace umlaut characters in heading IDs so that
        # weasyprint can resolve them as internal PDF links.
        local lua_filter="--lua-filter=normalize_ids.lua"
    else
        local title_template="title.html"
        local month="$MONTH_EN"
        local meta_title="Morserino-32 User Manual"
        local meta_lang="en"
        local lua_filter=""
    fi

    echo ""
    echo "=== Building language: $lang ==="

    if [ ! -f "$input" ]; then
        echo "ERROR: $input not found in $(pwd)"
        return 1
    fi

    if [ ! -f "$title_template" ]; then
        echo "ERROR: $title_template not found in $(pwd)"
        return 1
    fi

    if [ "$lang" = "de" ] && [ ! -f "normalize_ids.lua" ]; then
        echo "ERROR: normalize_ids.lua not found in $(pwd)"
        return 1
    fi

    local title_file
    title_file=$(resolve_title "$title_template" "$month") || return 1
    trap 'rm -f "$title_file"' RETURN

    echo "Building HTML from $input (V${MAJOR}.x, ${month} ${YEAR})..."

    pandoc "$input" \
        -o "$html_output" \
        --standalone \
        --toc \
        --toc-depth=3 \
        --number-sections \
        --css="$STYLE" \
        --resource-path=".:images" \
        --metadata title="$meta_title" \
        --metadata lang="$meta_lang" \
        --wrap=none \
        --from markdown+fenced_divs \
        --include-before-body="$title_file" \
        $lua_filter \
        2>&1

    if [ $? -ne 0 ]; then
        echo "ERROR: pandoc failed for $lang"
        return 1
    fi

    echo "Created $html_output"

    if [ "$format" = "html" ]; then
        echo "Done ($lang HTML only)"
        return 0
    fi

    echo "Converting to PDF with weasyprint..."

    weasyprint "$html_output" "$pdf_output" 2>&1

    if [ $? -eq 0 ]; then
        echo "Successfully created $pdf_output"
        return 0
    else
        echo "ERROR: weasyprint failed for $lang"
        echo "Make sure weasyprint is installed: pip install weasyprint"
        return 1
    fi
}

# Build one or both languages, collect results
run_build() {
    local format=$1
    shift
    local langs=("$@")
    local ok=()
    local fail=()

    for lang in "${langs[@]}"; do
        build_pdf "$lang" "$format"
        if [ $? -eq 0 ]; then
            ok+=("$lang")
        else
            fail+=("$lang")
        fi
    done

    echo ""
    echo "=== Build summary ==="
    for l in "${ok[@]}"; do
        if [ "$format" = "html" ]; then
            echo "  OK:   manual_${l}.html"
        else
            echo "  OK:   m32UserManual_v${MAJOR}_${l}.pdf"
        fi
    done
    for l in "${fail[@]}"; do echo "  FAIL: $l"; done

    [ ${#fail[@]} -eq 0 ]
}

case "${1:-all}" in
    en)
        run_build "${2:-pdf}" en
        ;;
    de)
        run_build "${2:-pdf}" de
        ;;
    all)
        run_build "${2:-pdf}" en de
        ;;
    *)
        echo "Usage: $0 [en|de|all] [html|pdf]"
        exit 1
        ;;
esac
