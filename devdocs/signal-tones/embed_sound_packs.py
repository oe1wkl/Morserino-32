#!/usr/bin/env python3
"""Embed the sound packs into the Configuration Tool.

The packs under "Documentation/Sound Packs/" are made by make_jingles.py next
door. This script base64-encodes them into m32_config_tool.html so the tool can
offer them directly -- preview in the browser, one click to install -- instead
of sending the user to GitHub to download two files by hand.

Why embedded rather than fetched:

  * The tool is one self-contained file. The iOS app (Software/iOS/M32Config)
    loads that same file unmodified in a WKWebView, and the published copy on
    morserino.info is placed by a script with a hand-maintained file allowlist.
    Anything fetched at run time would have to be added to both, and a miss is
    a silent 404 on one surface only.
  * Preview needs the bytes in the page anyway (an <audio> data: URI), and
    picking between five packs without hearing them is not much of an offer.
  * All ten files come to ~118 KB, ~157 KB once base64-encoded. That is small
    enough that removing every network dependency is the better trade.

Usage:
    python3 embed_sound_packs.py            # rewrite the generated block
    python3 embed_sound_packs.py --check    # exit 1 if the block is stale

--check is what CI runs: the embedded copy is generated data, and generated
data that can drift from its source silently is how this repository has been
bitten before (see the manual HTML/PDF gate in .github/workflows/pio-ci.yml).
"""

import base64
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
PACK_DIR = REPO / "Documentation" / "Sound Packs"
TOOL = REPO / "Software" / "Utilities" / "m32_config_tool.html"

BEGIN = "// === BEGIN GENERATED SOUND PACKS (devdocs/signal-tones/embed_sound_packs.py) ==="
END = "// === END GENERATED SOUND PACKS ==="

# Display name and one-line character for each pack, in the order the picker
# shows them: closest-to-the-built-in-signals first. The blurbs are the
# "character" column of Documentation/Sound Packs/README.md, shortened to fit a
# card. Adding a pack directory without adding it here is an error rather than a
# silent omission -- see check_packs().
PACKS = [
    ("fanfare", "Fanfare", "Closest to the built-in signals, with a bit more of an occasion made of it"),
    ("bells",   "Bells",   "Bright and ringing, clearly audible across a room"),
    ("marimba", "Marimba", "Dry and short, if you find a ringing tone intrusive between repetitions"),
    ("chime",   "Chime",   "Slow and quiet, for a quiet room or if any beep startles you"),
    ("arcade",  "Arcade",  "Fast chiptune, for the games"),
]


def check_packs():
    """Fail loudly when the directories and the table above disagree."""
    on_disk = {p.name for p in PACK_DIR.iterdir() if p.is_dir()}
    listed = {pack_id for pack_id, _, _ in PACKS}
    if on_disk != listed:
        missing = sorted(on_disk - listed)
        extra = sorted(listed - on_disk)
        msg = []
        if missing:
            msg.append("pack folder(s) with no entry in PACKS: " + ", ".join(missing))
        if extra:
            msg.append("PACKS entries with no folder: " + ", ".join(extra))
        sys.exit("ERROR: " + "; ".join(msg))


def b64(pack_id, name):
    path = PACK_DIR / pack_id / name
    if not path.is_file():
        sys.exit(f"ERROR: missing {path.relative_to(REPO)}")
    return base64.b64encode(path.read_bytes()).decode("ascii")


def build_block():
    lines = [
        BEGIN,
        "// Generated -- do not edit by hand. Re-run the script above after changing",
        '// anything under "Documentation/Sound Packs/".',
        "var SOUND_PACKS=[",
    ]
    total = 0
    for pack_id, name, blurb in PACKS:
        ok, err = b64(pack_id, "success.mp3"), b64(pack_id, "error.mp3")
        total += len(ok) + len(err)
        lines.append('{id:"%s",name:"%s",blurb:"%s",' % (pack_id, name, blurb))
        lines.append(' success:"%s",' % ok)
        lines.append(' error:"%s"},' % err)
    lines.append("];")
    lines.append(END)
    return "\n".join(lines), total


def splice(html, block):
    start = html.index(BEGIN)
    stop = html.index(END) + len(END)
    return html[:start] + block + html[stop:]


def main():
    check_packs()
    block, total = build_block()
    html = TOOL.read_text(encoding="utf-8")

    if BEGIN not in html or END not in html:
        sys.exit(f"ERROR: markers not found in {TOOL.relative_to(REPO)} -- "
                 "the generated block must stay delimited by them.")

    updated = splice(html, block)
    if "--check" in sys.argv:
        if updated != html:
            sys.exit("ERROR: the sound packs embedded in m32_config_tool.html are stale.\n"
                     "       Run: python3 devdocs/signal-tones/embed_sound_packs.py")
        print(f"OK: embedded sound packs match the files ({len(PACKS)} packs, "
              f"{total // 1024} KB of base64).")
        return

    if updated == html:
        print("Already up to date.")
        return
    TOOL.write_text(updated, encoding="utf-8")
    print(f"Embedded {len(PACKS)} packs ({total // 1024} KB of base64) into "
          f"{TOOL.relative_to(REPO)}.")


if __name__ == "__main__":
    main()
