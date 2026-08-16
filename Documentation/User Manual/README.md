# Morserino-32 User Manual

## Get the current manual

These two links always resolve to the manual for the **latest released
firmware**. They do not change when a new version comes out, so they are safe to
bookmark, to put on a web page, or to post to the user group:

- **English —** <https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_EN.pdf>
- **Deutsch —** <https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_DE.pdf>

The version and the month the manual was produced are printed on its title page.

Beta releases are not picked up by these links — they always give you the
current stable manual.

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

Needs `pandoc`, `weasyprint` and the Lato font.

```bash
cd "Version 9.x"
./build.sh            # both languages, PDF
./build.sh en html    # English, HTML only
```

## Maintenance scripts

| Script | What it does |
|---|---|
| `new-major-version.sh` | scaffolds the folder for a new major version from the previous one. `--check` (run by CI) verifies the folder matches the firmware's `VERSION_MAJOR`. |
| `sync_whatsnew.py` | regenerates the preface's "What is new" section from the change history in `Software/README.md`. `--check` (run by CI) reports when the two have drifted, in either language. |

Other languages: see `../Versions in French and Spanish/`.
