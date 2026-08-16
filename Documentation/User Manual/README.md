# Morserino-32 User Manual

## Get the current manual

The manual is built once per Morserino, so you only read what applies to your
device. These links always resolve to the manual for the **latest released
firmware** — they never change, so they are safe to bookmark, to put on a web
page, or to post to the user group.

| Your Morserino | English | Deutsch |
|---|---|---|
| **Morserino-32 Pocket** | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_EN.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_EN.epub) | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_DE.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_DE.epub) |
| **Morserino-32, 1st / 2nd edition** | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_EN.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_EN.epub) | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_DE.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_DE.epub) |
| **Pocket, Accessibility Edition** | [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_EN.epub) · [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_EN.pdf) | [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_DE.epub) · [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_DE.pdf) |

The version, the month and which edition it is are printed on the title page.

**Which one do I want?** The Pocket is the newer, smaller model with a colour
screen; the 1st and 2nd editions are the earlier ones with the small
monochrome display and a LoRa antenna socket. The **Accessibility Edition**
matches the firmware of the same name, which speaks the interface aloud for
blind and partially sighted operators — EPUB is listed first there because a
screen reader and a braille display handle it far better than a PDF.

Beta releases are not picked up by these links: they always give you the
current stable manual.

A combined manual covering all three models is still built and kept in the
repository (`m32UserManual_v<N>_{en,de}.pdf`), but it is not part of a
release.

## Why there is a folder per version

The manual is versioned **in parallel with the firmware**, not with the
hardware: `Version 9.x/` documents firmware 9.x, and one manual covers the
whole of that major version. All three Morserino generations — 1st edition, 2nd
edition and the M32 Pocket — are covered by the same manual.

Older folders are kept so that someone still running older firmware can find
the manual that matches it. **The highest-numbered folder is the current one**,
but you rarely need to know that: use the links above.

Each folder holds the sources as well as the built manual:

| File | What it is |
|---|---|
| `manual_en.md`, `manual_de.md` | the sources — this is what gets edited |
| `m32UserManual_v<N>_en.pdf`, `..._de.pdf` | the built manuals |
| `manual_en.html`, `manual_de.html` | the same content as HTML |
| `build.sh` | builds both, via pandoc + weasyprint |
| `title.html`, `title_de.html` | title-page templates; version and date are filled in at build time |
| `style.css`, `normalize_ids.lua`, `images/` | print stylesheet, German heading-ID filter, figures |

## Building it yourself

Needs `pandoc`; `weasyprint` and the Lato font for PDF; `epubcheck`
(`brew install epubcheck`) to validate EPUBs — without it they are still built,
just checked less thoroughly.

```bash
cd "Version 9.x"
./build.sh                     # both languages, PDF, all models
./build.sh en html             # English, HTML only
./build.sh en pdf pocket       # just the Pocket manual
./build.sh all epub all        # every language x every model, as EPUB
```

The third argument selects the hardware variant — `combined` (the default, all
models), `classic`, `pocket`, `pocket-a11y`, or `all`. Variants are produced by
filtering one tagged source with `variant.lua`; see
[`devdocs/manual-variants/`](../../devdocs/manual-variants/).

## Maintenance scripts

| Script | What it does |
|---|---|
| `new-major-version.sh` | scaffolds the folder for a new major version from the previous one. `--check` (run by CI) verifies the folder matches the firmware's `VERSION_MAJOR`. |
| `sync_whatsnew.py` | regenerates the preface's "What is new" section from the change history in `Software/README.md`. `--check` (run by CI) reports when the two have drifted, in either language. |

Other languages: see `../Versions in French and Spanish/`.
