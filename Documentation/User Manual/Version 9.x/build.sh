#!/bin/bash
#
# Build script for Morserino-32 User Manual
# Generates PDF via HTML + weasyprint
#
# Requirements:
#   - pandoc
#   - weasyprint (pip install weasyprint)      for PDF
#   - Lato font installed                      for PDF
#   - epubcheck (brew install epubcheck)       for EPUB; optional, but then the
#                                              EPUBs are only syntax-checked
#
# Usage:
#   ./build.sh                     # both languages, PDF, combined manual
#   ./build.sh en                  # English PDF
#   ./build.sh de                  # German PDF
#   ./build.sh all                 # both languages
#   ./build.sh en html             # English HTML only
#   ./build.sh en epub             # English EPUB
#
#   ./build.sh en pdf pocket       # English Pocket manual
#   ./build.sh all pdf all         # every language x every variant (8 PDFs)
#
# The third argument selects the hardware variant, and defaults to 'combined' —
# the manual as it has always been built, covering all three Morserinos:
#
#   combined      every variant's content, nothing filtered out (default)
#   classic       Morserino-32 1st and 2nd edition (OLED)
#   pocket        Morserino-32 Pocket (TFT)
#   pocket-a11y   Pocket accessibility edition
#   all           combined plus all three
#
# Filtering is done by variant.lua against the tags in the Markdown sources.
# Combined output keeps the established filenames (m32UserManual_v<N>_en.pdf);
# a variant adds its key (m32UserManual_v<N>_en_pocket.pdf).
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
ISO_MONTH=$(date "+%Y-%m")      # dc:date in EPUB must be ISO-8601, or pandoc drops it
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

# How each variant names itself on the title page. The title page is injected
# with --include-before-body and so never passes through variant.lua, which is
# why the name is substituted here rather than tagged in the sources.
variant_name() {
    local lang=$1 variant=$2
    case "$lang:$variant" in
        en:combined)    echo "for all Morserino-32 models" ;;
        en:classic)     echo "for the Morserino-32, 1st and 2nd edition" ;;
        en:pocket)      echo "for the Morserino-32 Pocket" ;;
        en:pocket-a11y) echo "for the Morserino-32 Pocket — Accessibility Edition" ;;
        de:combined)    echo "für alle Morserino-32-Modelle" ;;
        de:classic)     echo "für den Morserino-32, 1. und 2. Edition" ;;
        de:pocket)      echo "für den Morserino-32 Pocket" ;;
        de:pocket-a11y) echo "für den Morserino-32 Pocket — Accessibility Edition" ;;
        *)              echo "" ;;
    esac
}

# The short form of the variant name, for places where a whole phrase does not
# fit -- an EPUB title in a reader's library, for instance.
variant_label() {
    local lang=$1 variant=$2
    case "$lang:$variant" in
        en:classic)     echo "1st and 2nd edition" ;;
        en:pocket)      echo "Pocket" ;;
        en:pocket-a11y) echo "Pocket, Accessibility Edition" ;;
        de:classic)     echo "1. und 2. Edition" ;;
        de:pocket)      echo "Pocket" ;;
        de:pocket-a11y) echo "Pocket, Accessibility Edition" ;;
        *)              echo "" ;;
    esac
}

# Fill the title-page template for one language; prints the temp file's path.
resolve_title() {
    local template=$1 month=$2 vname=$3 out
    out=$(mktemp "${TMPDIR:-/tmp}/m32title.XXXXXX.html") || return 1
    # The BUILD-TEMPLATE comment explains the placeholders to whoever edits the
    # template; it is not for readers, so it never reaches the published HTML.
    sed -e '/<!-- BUILD-TEMPLATE/,/-->/d' \
        -e "s|@MAJOR@|${MAJOR}|g" \
        -e "s|@MONTH_YEAR@|${month} ${YEAR}|g" \
        -e "s|@YEAR@|${YEAR}|g" \
        -e "s|@VARIANT@|${vname}|g" \
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
    local variant=${3:-combined}
    local input="manual_${lang}.md"
    local suffix=""
    if [ "$variant" != "combined" ]; then
        suffix="_${variant}"
    fi
    local html_output="manual_${lang}${suffix}.html"
    local pdf_output="m32UserManual_v${MAJOR}_${lang}${suffix}.pdf"
    local epub_output="m32UserManual_v${MAJOR}_${lang}${suffix}.epub"

    local variant_filter=()
    if [ "$variant" != "combined" ]; then
        if [ ! -f "variant.lua" ]; then
            echo "ERROR: variant.lua not found in $(pwd)"
            return 1
        fi
        variant_filter=(--lua-filter=variant.lua --metadata "variant=$variant")
    fi

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
    echo "=== Building language: $lang ($variant) ==="

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
    title_file=$(resolve_title "$title_template" "$month" \
                              "$(variant_name "$lang" "$variant")") || return 1
    trap 'rm -f "$title_file"' RETURN

    if [ "$format" = "epub" ]; then
        # EPUB is generated straight from the Markdown. It deliberately does not
        # reuse the print title page (absolutely positioned, in centimetres) or
        # style.css (@page rules): a reflowable book has no pages to lay out.
        # The variant name becomes the subtitle, so an EPUB reader shows which
        # manual this is in its library without opening it.
        # A reader's library shows the title and little else, so a variant
        # manual has to say which one it is there -- otherwise three EPUBs on a
        # shelf are indistinguishable.
        local epub_title="$meta_title"
        local epub_desc="Version ${MAJOR}.x, ${month} ${YEAR}"
        local label
        label=$(variant_label "$lang" "$variant")
        if [ -n "$label" ]; then
            epub_title="$meta_title ($label)"
            epub_desc="$epub_desc — $label"
        fi
        echo "Building EPUB from $input (V${MAJOR}.x, ${month} ${YEAR})..."
        pandoc "$input" \
            -o "$epub_output" \
            --toc \
            --toc-depth=3 \
            --number-sections \
            --css=epub.css \
            --resource-path=".:images" \
            --metadata title="$epub_title" \
            --metadata author="Willi Kraml, OE1WKL" \
            --metadata lang="$meta_lang" \
            --metadata date="$ISO_MONTH" \
            --metadata description="$epub_desc" \
            --metadata rights="© 2016-${YEAR} Willi Kraml, OE1WKL. CC BY 4.0" \
            --epub-cover-image=images/M32_logo.png \
            --wrap=none \
            --from markdown+fenced_divs \
            "${variant_filter[@]}" \
            $lua_filter \
            2>&1 || { echo "ERROR: pandoc failed for $lang (epub)"; return 1; }
        # Validate before anyone reads it. An EPUB is a zip of XHTML, and
        # XHTML is XML: pandoc passes raw HTML through untouched and never
        # checks it, so a raw <br> or a "--" inside an HTML comment produces a
        # file that a reader refuses to render. Willi found exactly that in
        # Apple Books, which is later than anyone should find out.
        #
        # epubcheck is the real validator -- it checks the package, the
        # metadata, the spine and the XHTML. --locale en keeps the output the
        # same on a German Mac as in a CI log. --failonwarnings is deliberate:
        # the manuals are at zero warnings today, so any new one is worth
        # stopping for. If a benign warning ever blocks a release, drop that
        # flag rather than ignore the output.
        #
        # Without epubcheck (it needs Java) fall back to parsing the XHTML with
        # python, which still catches the malformed-markup class of error.
        if command -v epubcheck >/dev/null 2>&1; then
            echo "Validating $epub_output with epubcheck..."
            if ! epubcheck --locale en --failonwarnings "$epub_output"; then
                echo "ERROR: epubcheck rejected $epub_output"
                echo "   Two causes account for nearly all markup failures:"
                echo "     - a raw <br> in the Markdown, which has to be <br/>;"
                echo "     - a '--' inside an HTML comment, which XML forbids."
                rm -f "$epub_output"
                return 1
            fi
        elif command -v python3 >/dev/null 2>&1; then
            echo "  (epubcheck not found - falling back to an XHTML syntax check)"
            if ! python3 - "$epub_output" <<'PYCHECK'
import sys, zipfile, xml.etree.ElementTree as ET
bad = []
with zipfile.ZipFile(sys.argv[1]) as z:
    for name in z.namelist():
        if name.endswith((".xhtml", ".html")):
            try:
                ET.fromstring(z.read(name))
            except ET.ParseError as exc:
                bad.append((name, exc))
if bad:
    print("ERROR: %d file(s) in the EPUB are not well-formed XHTML:" % len(bad))
    for name, exc in bad:
        print("   %s: %s" % (name, exc))
    print("   Two causes account for nearly all of these:")
    print("     - a raw <br> in the Markdown, which has to be <br/>;")
    print("     - a '--' inside an HTML comment, which XML forbids.")
    sys.exit(1)
PYCHECK
            then
                rm -f "$epub_output"
                return 1
            fi
        else
            echo "  (neither epubcheck nor python3 found - EPUB not validated)"
        fi

        echo "Successfully created $epub_output"
        return 0
    fi

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
        "${variant_filter[@]}" \
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

# Build every requested language x variant combination, collect results
run_build() {
    local format=$1 variants_arg=$2
    shift 2
    local langs=("$@")
    local variants=()
    local ok=() fail=()

    if [ "$variants_arg" = "all" ]; then
        variants=(combined classic pocket pocket-a11y)
    else
        variants=("$variants_arg")
    fi

    for variant in "${variants[@]}"; do
        for lang in "${langs[@]}"; do
            local suffix=""
            [ "$variant" != "combined" ] && suffix="_${variant}"
            if build_pdf "$lang" "$format" "$variant"; then
                case "$format" in
                    html) ok+=("manual_${lang}${suffix}.html") ;;
                    epub) ok+=("m32UserManual_v${MAJOR}_${lang}${suffix}.epub") ;;
                    *)    ok+=("m32UserManual_v${MAJOR}_${lang}${suffix}.pdf") ;;
                esac
            else
                fail+=("${lang} (${variant})")
            fi
        done
    done

    echo ""
    echo "=== Build summary ==="
    for f in "${ok[@]}"; do echo "  OK:   $f"; done
    for f in "${fail[@]}"; do echo "  FAIL: $f"; done

    [ ${#fail[@]} -eq 0 ]
}

VARIANT="${3:-combined}"
case "$VARIANT" in
    combined|classic|pocket|pocket-a11y|all) ;;
    *)
        echo "ERROR: unknown variant '$VARIANT'."
        echo "       Expected combined, classic, pocket, pocket-a11y or all."
        exit 1 ;;
esac

case "${1:-all}" in
    en)  run_build "${2:-pdf}" "$VARIANT" en ;;
    de)  run_build "${2:-pdf}" "$VARIANT" de ;;
    all) run_build "${2:-pdf}" "$VARIANT" en de ;;
    *)
        echo "Usage: $0 [en|de|all] [html|pdf|epub] [combined|classic|pocket|pocket-a11y|all]"
        exit 1
        ;;
esac
