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
#   ./build.sh              # build English PDF
#   ./build.sh de           # build German PDF
#   ./build.sh all          # build both
#   ./build.sh en html      # build English HTML only
#

STYLE="style.css"
TITLE="title.html"

build_pdf() {
    local lang=$1
    local format=${2:-pdf}
    local input="manual_${lang}.md"
    local html_output="manual_${lang}.html"
    local pdf_output="m32UserManual_v8_${lang}.pdf"
    
    if [ ! -f "$input" ]; then
        echo "Error: $input not found"
        exit 1
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
        --metadata title="Morserino-32 User Manual" \
        --wrap=none \
        --from markdown+fenced_divs \
        --include-before-body="$TITLE" \
        2>&1
    
    if [ $? -ne 0 ]; then
        echo "Error generating HTML"
        exit 1
    fi
    
    echo "Created $html_output"
    
    if [ "$format" = "html" ]; then
        echo "Done (HTML only)"
        return
    fi
    
    echo "Converting to PDF with weasyprint..."
    
    weasyprint "$html_output" "$pdf_output" 2>&1
    
    if [ $? -eq 0 ]; then
        echo "Successfully created $pdf_output"
    else
        echo "Error creating PDF. Make sure weasyprint is installed:"
        echo "  pip install weasyprint"
        exit 1
    fi
}

case "${1:-en}" in
    en)
        build_pdf en ${2:-pdf}
        ;;
    de)
        build_pdf de ${2:-pdf}
        ;;
    all)
        build_pdf en
        build_pdf de
        ;;
    *)
        echo "Usage: $0 [en|de|all] [html|pdf]"
        exit 1
        ;;
esac
