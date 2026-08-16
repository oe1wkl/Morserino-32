#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sync_whatsnew.py -- keep the manual's "What is new" section in step with
Software/README.md.

The preface of every manual carries a short synopsis of what the current major
version brought. It used to be retyped by hand from the change history, which
is how a V9 manual ended up with a section headed "What is new in Version 8?".

This script generates that section instead, from the change history itself:

  * every '### CHANGES V. <major>.x' section in Software/README.md, newest
    version first (so a V9 manual covers 9.0, 9.1, 9.2 ... cumulatively);
  * from each, the '#### New Features:' and '#### Feature Modifications:'
    bullets. Bug Fixes are skipped, as are Other Changes and Notes -- those are
    developer-facing (heap handling, code clean-up) and have no place in a user
    manual;
  * rendered into the manual between WHATSNEW markers, in the manual's own
    bullet style.

German
------
Software/README.md is English only, so the German block cannot be generated.
It stays a translation, and this script tracks whether it is still current: the
marker records a fingerprint of the English text it was translated from, so
--check can say "the German what's-new is stale" instead of everyone finding
out at release time.

Usage
-----
  sync_whatsnew.py --check          # report drift, change nothing (CI, exit 1)
  sync_whatsnew.py --write          # regenerate the English block
  sync_whatsnew.py --write --lang de --from-en
                                    # after translating: re-stamp the German
                                    # block as current for today's English text
  sync_whatsnew.py --print          # just show what would be generated

  --major N     override the major version (default: VERSION_MAJOR from
                morsedefs.h, which is also how the manual folder is found)
"""
import argparse
import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
README = os.path.join(REPO, "Software", "README.md")
MORSEDEFS = os.path.join(REPO, "Software", "src", "Version 6 and newer",
                         "morsedefs.h")

BEGIN = "<!-- WHATSNEW:BEGIN"
END = "<!-- WHATSNEW:END -->"

HEADING = {"en": "What is new in Version %s?",
           "de": "Was ist neu in Version %s?"}
# Only these two subsections reach the manual.
WANTED = ("New Features", "Feature Modifications")


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


# ---------------------------------------------------------------- extraction
def changelog_sections(text, major):
    """[(version, {subsection: [bullets]})], newest version first."""
    # Headings are inconsistent in that file: 'CHANGES V. 9.0', 'Changes V. 8.0',
    # 'Changes V.7.1'. Tolerate all of them, as extract_changelog.py does.
    head = re.compile(r"^###\s+Changes\s+V\.?\s*(%s\.\d+(?:\.\d+)?)\s*$" % major,
                      re.MULTILINE | re.IGNORECASE)
    any_head = re.compile(r"^###\s", re.MULTILINE)
    out = []
    for m in head.finditer(text):
        start = m.end()
        nxt = any_head.search(text, start)
        body = text[start:nxt.start() if nxt else len(text)]
        out.append((m.group(1), subsections(body)))
    out.sort(key=lambda t: [int(p) for p in t[0].split(".")], reverse=True)
    return out


def subsections(body):
    """{subsection name: [bullet text]} for the wanted subsections only."""
    found, current = {}, None
    for line in body.split("\n"):
        h = re.match(r"^####\s+(.*?):?\s*$", line)
        if h:
            name = h.group(1).strip()
            current = name if name in WANTED else None
            if current:
                found.setdefault(current, [])
            continue
        if current is None:
            continue
        b = re.match(r"^\s*[*-]\s+(.*)$", line)
        if b:
            found[current].append(b.group(1).rstrip())
        elif line.strip() and found.get(current):
            # continuation line of the previous bullet
            found[current][-1] += " " + line.strip()
    return found


def escape_angles(text):
    """Escape '<' so prosigns and placeholders survive into the PDF.

    The change history writes prosigns and protocol placeholders bare: '<AR>',
    '<err>', '<n>', '<content>', and even '<b>' / '</b>'. Pandoc reads those as
    raw inline HTML, so '<AR>' renders as *nothing at all* and '<b>' silently
    turns on bold for the rest of the paragraph. The manual's own convention is
    the backslash escape ('\\<HH>', '\\<COMx>'), so use that. Autolinks are left
    alone -- '<https://...>' is deliberate markdown.
    """
    return re.sub(r"<(?!https?://)", r"\\<", text)


def render(sections, major, lang):
    """The manual-ready block body (without the markers)."""
    lines = [HEADING[lang] % major, ""]
    multi = len(sections) > 1
    for version, subs in sections:
        bullets = []
        for name in WANTED:
            bullets.extend(subs.get(name, []))
        if not bullets:
            continue
        if multi:
            lines.append("**V %s**" % version)
            lines.append("")
        for b in bullets:
            # the manual's own bullet style; wrap nothing, --wrap=none anyway
            lines.append("-   %s" % escape_angles(b))
        lines.append("")
    while lines and not lines[-1]:
        lines.pop()
    return "\n".join(lines)


def fingerprint(text):
    return hashlib.sha256(re.sub(r"\s+", " ", text).strip()
                          .encode("utf-8")).hexdigest()[:12]


# ------------------------------------------------------------------- markers
def find_block(text, path):
    """(start, end, attrs, body) of the WHATSNEW block."""
    b = text.find(BEGIN)
    if b < 0:
        sys.exit("%s has no %s ... %s markers -- add them around the existing\n"
                 "'What is new' section once, then this script maintains it."
                 % (os.path.basename(path), BEGIN + " -->", END))
    close = text.index("-->", b) + 3
    e = text.index(END, close)
    attrs = dict(re.findall(r"(\w+)=(\S+)", text[b:close]))
    return b, e + len(END), attrs, text[close:e].strip("\n")


def write_block(path, body, attrs):
    text = open(path, encoding="utf-8").read()
    b, e, _, _ = find_block(text, path)
    marker = BEGIN + "".join(" %s=%s" % kv for kv in sorted(attrs.items())) + " -->"
    open(path, "w", encoding="utf-8").write(
        text[:b] + marker + "\n" + body + "\n" + END + text[e:])


# ---------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--print", dest="show", action="store_true")
    ap.add_argument("--lang", choices=("en", "de"), default="en")
    ap.add_argument("--from-en", action="store_true",
                    help="with --write --lang de: keep the German text, only "
                         "re-stamp it as translated from today's English block")
    ap.add_argument("--major")
    args = ap.parse_args()
    if not (args.check or args.write or args.show):
        ap.error("choose one of --check, --write, --print")

    major = args.major or firmware_major()
    mdir = manual_dir(major)
    readme = open(README, encoding="utf-8").read()
    sections = changelog_sections(readme, major)
    if not sections:
        sys.exit("no '### Changes V%s.x' section in Software/README.md -- "
                 "nothing to synchronise." % major)

    body_en = render(sections, major, "en")
    stamp = fingerprint(body_en)

    if args.show:
        print(body_en)
        return 0

    en_path = os.path.join(mdir, "manual_en.md")
    de_path = os.path.join(mdir, "manual_de.md")

    if args.write:
        if args.lang == "en":
            write_block(en_path, body_en, {"v": major, "en": stamp})
            print("manual_en.md: what's-new rewritten from Software/README.md "
                  "(%d version section(s), fingerprint %s)"
                  % (len(sections), stamp))
            print("Now translate that block into manual_de.md, then run:")
            print("  sync_whatsnew.py --write --lang de --from-en")
        else:
            if not args.from_en:
                sys.exit("the German block is a translation and cannot be "
                         "generated.\nTranslate it by hand, then re-run with "
                         "--from-en to stamp it as current.")
            text = open(de_path, encoding="utf-8").read()
            _, _, _, de_body = find_block(text, de_path)
            write_block(de_path, de_body, {"v": major, "en": stamp})
            print("manual_de.md: stamped as translated from English %s" % stamp)
        return 0

    # --check
    problems = []
    en_text = open(en_path, encoding="utf-8").read()
    _, _, en_attrs, en_body = find_block(en_text, en_path)
    if en_body.strip() != body_en.strip():
        problems.append(
            "manual_en.md: the what's-new block does not match "
            "Software/README.md.\n      Run: sync_whatsnew.py --write")
    if en_attrs.get("v") != major:
        problems.append("manual_en.md: block is stamped for V%s, firmware is V%s."
                        % (en_attrs.get("v"), major))

    de_text = open(de_path, encoding="utf-8").read()
    _, _, de_attrs, _ = find_block(de_text, de_path)
    if de_attrs.get("en") != stamp:
        problems.append(
            "manual_de.md: translated from English %s, but the English text is "
            "now %s.\n      Re-translate the block, then: sync_whatsnew.py "
            "--write --lang de --from-en"
            % (de_attrs.get("en", "?"), stamp))

    if problems:
        print("What's-new is out of date (%d):" % len(problems))
        for p in problems:
            print("  FAIL: %s" % p)
        return 1
    print("OK: what's-new matches Software/README.md in both languages "
          "(V%s, %d section(s), fingerprint %s)."
          % (major, len(sections), stamp))
    return 0


if __name__ == "__main__":
    sys.exit(main())
