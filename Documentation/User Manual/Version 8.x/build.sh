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

STYLE="style.css"

build_pdf() {
    local lang=$1
    local format=${2:-pdf}
    local input="manual_${lang}.md"
    local html_output="manual_${lang}.html"
    local pdf_output="m32UserManual_v8_${lang}.pdf"

    # Language-specific settings
    if [ "$lang" = "de" ]; then
        local title_file="title_de.html"
        local meta_title="Morserino-32 Benutzerhandbuch"
        local meta_lang="de"
        # Lua filter to replace umlaut characters in heading IDs so that
        # weasyprint can resolve them as internal PDF links.
        local lua_filter="--lua-filter=normalize_ids.lua"
    else
        local title_file="title.html"
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

    if [ ! -f "$title_file" ]; then
        echo "ERROR: $title_file not found in $(pwd)"
        return 1
    fi

    if [ "$lang" = "de" ] && [ ! -f "normalize_ids.lua" ]; then
        echo "ERROR: normalize_ids.lua not found in $(pwd)"
        return 1
    fi

    echo "Building HTML from $input..."

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
    for l in "${ok[@]}";   do echo "  OK:   m32UserManual_v8_${l}.pdf"; done
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
