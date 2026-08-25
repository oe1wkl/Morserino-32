#!/usr/bin/env python3
"""
check_manual_fresh.py -- was the built manual made from the sources as they are now?

The manual's HTML and PDF are generated files that are nevertheless committed,
and nothing ever forced them to be regenerated when their sources changed. So
they drifted: on 2026-08-24 the committed V9 PDFs turned out to be missing a
whole section that had been sitting in manual_*.md for over a week, along with
three "What is new" entries. Nobody had done anything wrong -- there was simply
no gate, and a stale PDF looks exactly like a current one.

Rebuilding in CI and diffing against the committed files does NOT work as a
gate. The title page carries the month of the build, so a rebuild in a new
month differs from the committed file even when nothing changed, and PDFs are
not reproducible byte for byte across tool versions anyway.

So this works the way the "What is new" gate does, and for the same reason:
build.sh stamps the generated HTML with a fingerprint of the sources it was
made from, and this recomputes that fingerprint from the sources in the tree
and compares. A stale artefact is then a mismatch, not a guess.

Only the *combined* manuals are checked, because only those are committed; the
per-variant PDFs and the EPUBs are release artefacts and are built on demand.

Usage:
  check_manual_fresh.py --check        verify both languages (this is what CI runs)
  check_manual_fresh.py --stamp en     record the current sources in manual_en.html
                                       (build.sh calls this after the PDF is written)

  --major N   override the major version (default: VERSION_MAJOR from morsedefs.h)
"""

import argparse
import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
MORSEDEFS = os.path.join(REPO, "Software", "src", "Version 6 and newer",
                         "morsedefs.h")

STAMP_RE = re.compile(r"<!-- MANUALFRESH: lang=(\w+) src=([0-9a-f]+) -->\n?")

# Everything that changes what the built manual contains. Deliberately NOT
# build.sh itself: editing a comment in it would then demand a rebuild of both
# languages, and the flags that actually matter are few and rarely touched.
SHARED = ("style.css", "variant.lua", "normalize_ids.lua")
PER_LANG = {"en": ("manual_en.md", "title.html"),
            "de": ("manual_de.md", "title_de.html")}
LANGS = ("en", "de")


def firmware_major():
    with open(MORSEDEFS, encoding="utf-8") as fh:
        for line in fh:
            m = re.match(r"#define\s+VERSION_MAJOR\s+(\d+)", line)
            if m:
                return m.group(1)
    sys.exit("could not read VERSION_MAJOR from %s" % MORSEDEFS)


def manual_dir(major):
    d = os.path.join(REPO, "Documentation", "User Manual", "Version %s.x" % major)
    if not os.path.isdir(d):
        sys.exit("manual directory not found: %s" % d)
    return d


def source_files(d, lang):
    """Every input to the combined build for one language, in a fixed order."""
    files = [os.path.join(d, f) for f in PER_LANG[lang]]
    files += [os.path.join(d, f) for f in SHARED]
    images = os.path.join(d, "images")
    if os.path.isdir(images):
        files += [os.path.join(images, f) for f in sorted(os.listdir(images))
                  if not f.startswith(".")]
    return files


def fingerprint(paths):
    """Hash of the sources.

    Line endings are normalised so a checkout on another platform does not read
    as a change, but nothing else is: Markdown is whitespace-sensitive, and an
    indentation change really does alter how a list nests in the output.
    """
    h = hashlib.sha256()
    for p in paths:
        h.update(os.path.basename(p).encode("utf-8") + b"\0")
        if os.path.exists(p):
            with open(p, "rb") as fh:
                h.update(fh.read().replace(b"\r\n", b"\n"))
        else:
            h.update(b"<missing>")   # so adding the file later changes the hash
        h.update(b"\0")
    return h.hexdigest()[:12]


def read_stamp(path):
    if not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as fh:
        m = STAMP_RE.search(fh.read())
    return (m.group(1), m.group(2)) if m else None


def write_stamp(path, lang, fp):
    with open(path, encoding="utf-8") as fh:
        text = fh.read()
    text = STAMP_RE.sub("", text)
    stamp = "<!-- MANUALFRESH: lang=%s src=%s -->\n" % (lang, fp)
    if "</body>" in text:
        text = text.replace("</body>", stamp + "</body>", 1)
    else:
        text = text.rstrip("\n") + "\n" + stamp
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def rebuild_hint(d):
    return "  Rebuild with:  (cd '%s' && ./build.sh)" % os.path.relpath(d, REPO)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--stamp", choices=LANGS)
    ap.add_argument("--major")
    args = ap.parse_args()

    if not args.check and not args.stamp:
        ap.error("give --check or --stamp <lang>")

    major = args.major or firmware_major()
    d = manual_dir(major)

    if args.stamp:
        lang = args.stamp
        html = os.path.join(d, "manual_%s.html" % lang)
        if not os.path.exists(html):
            sys.exit("cannot stamp: %s does not exist" % html)
        write_stamp(html, lang, fingerprint(source_files(d, lang)))
        return 0

    fail = False
    for lang in LANGS:
        html = os.path.join(d, "manual_%s.html" % lang)
        pdf = os.path.join(d, "m32UserManual_v%s_%s.pdf" % (major, lang))
        label = "V%s.x %s" % (major, lang)

        if not os.path.exists(html):
            print("FAIL: %s -- manual_%s.html is missing." % (label, lang))
            print(rebuild_hint(d))
            fail = True
            continue
        if not os.path.exists(pdf):
            print("FAIL: %s -- %s is missing." % (label, os.path.basename(pdf)))
            print(rebuild_hint(d))
            fail = True
            continue

        want = fingerprint(source_files(d, lang))
        got = read_stamp(html)

        if got is None:
            print("FAIL: %s -- manual_%s.html carries no MANUALFRESH stamp, so it "
                  "was not produced by a full build." % (label, lang))
            print("      ('build.sh %s html' writes the HTML without one, on purpose: "
                  "it leaves the PDF behind.)" % lang)
            print(rebuild_hint(d))
            fail = True
        elif got[0] != lang:
            print("FAIL: %s -- manual_%s.html is stamped for '%s'." % (label, lang, got[0]))
            print(rebuild_hint(d))
            fail = True
        elif got[1] != want:
            print("FAIL: %s -- the built manual is older than its sources." % label)
            print("      sources now: %s, built from: %s" % (want, got[1]))
            print("      Someone edited manual_%s.md (or the stylesheet, title page,"
                  " a filter or an image) and committed it without rebuilding, so the"
                  " committed HTML and PDF do not contain that change." % lang)
            print(rebuild_hint(d))
            fail = True

    if fail:
        return 1
    print("OK: the built manuals match their sources (V%s, %s)."
          % (major, ", ".join(LANGS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
