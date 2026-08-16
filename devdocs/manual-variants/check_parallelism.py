#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_parallelism.py -- keep the English and German manuals structurally identical.

Briefing 6 makes this a hard requirement: the same tags, in the same places, in
the same order, in both languages. That is what makes drift between them
detectable by diff. This script is that diff.

Line numbers are useless for the comparison -- the two sources use different
wrapping conventions (EN has 169 lines over 90 characters, DE has 10), so they
are compared *structurally*:

  1. the heading tree must match one for one (count, level and order);
  2. within each section, the ordered sequence of variant tags must match,
     where a tag is (kind, classes) with kind in div / span / row;
  3. the note/important/warning/quote divs must match too -- a box present in
     one language and missing in the other is a real content divergence;
  4. no unclosed fenced div, and no class outside the agreed vocabulary.

Exit status is 0 when the two are parallel, 1 otherwise.

Usage:
    python3 check_parallelism.py
"""
import re
import sys

import tagmap


BOX_CLASSES = ("note", "important", "warning", "quote")


def boxes(lang):
    """note / important / warning / quote divs as (section index, class).

    Keyed on the section's *position*, never its title -- the titles are in
    different languages, so only the index is comparable.
    """
    path = tagmap.MANUAL + "/manual_%s.md" % lang
    out, index, titles = [], -1, []
    fenced = False
    for line in open(path, encoding="utf-8"):
        if line.startswith("```"):
            fenced = not fenced
            continue
        if fenced:
            continue
        h = re.match(r"^(#{1,6}) +(.*?)\s*(\{[^}]*\})?\s*$", line)
        if h:
            index += 1
            titles.append(h.group(2).strip())
        m = tagmap.DIV_OPEN.match(line)
        if m and not tagmap.DIV_CLOSE.match(line):
            cls = [c for c in tagmap._classes(m.group(2)) if c in BOX_CLASSES]
            if cls:
                out.append((index, cls[0]))
    return out, titles


def main():
    en, de = tagmap.parse("en"), tagmap.parse("de")
    problems = []

    for tm in (en, de):
        if tm["unclosed"]:
            problems.append("%s: fenced div never closed, opened at line(s) %s"
                            % (tm["lang"],
                               ", ".join(str(s[0]) for s in tm["unclosed"])))
        for rec in tm["blocks"]:
            if rec["end"] is None:
                problems.append("%s: div at line %d has no closing :::"
                                % (tm["lang"], rec["line"]))
            for c in tagmap.unknown_classes("{%s}" % " ".join(
                    "." + x for x in rec["all_classes"])):
                problems.append("%s line %d: unknown class .%s"
                                % (tm["lang"], rec["line"], c))

    # 1. heading tree
    if len(en["sections"]) != len(de["sections"]):
        problems.append("heading count differs: EN %d, DE %d"
                        % (len(en["sections"]), len(de["sections"])))
    for i, (a, b) in enumerate(zip(en["sections"], de["sections"])):
        if a["level"] != b["level"]:
            problems.append(
                "heading %d level differs: EN L%d '%s' (h%d) vs DE L%d '%s' (h%d)"
                % (i + 1, a["line"], a["title"], a["level"],
                   b["line"], b["title"], b["level"]))

    # 2. variant tags, section by section
    for a, b in zip(en["sections"], de["sections"]):
        sa = [(k, c) for k, c, _ in a["tags"]]
        sb = [(k, c) for k, c, _ in b["tags"]]
        if sa != sb:
            problems.append(
                "tags differ in section EN L%d '%s' / DE L%d '%s':\n"
                "        EN %s\n        DE %s"
                % (a["line"], a["title"], b["line"], b["title"],
                   sa or "(none)", sb or "(none)"))

    # 3. note / important / warning / quote boxes -- these are content, not
    #    tagging, so they are reported separately from tagging problems.
    divergences = []
    ba, ta = boxes("en")
    bb, tb = boxes("de")
    if ba != bb:
        import difflib
        for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(
                None, ba, bb).get_opcodes():
            if tag == "equal":
                continue
            divergences.append(
                "%s: EN %s / DE %s"
                % (tag,
                   ["<%s> in '%s'" % (c, ta[i]) for i, c in ba[i1:i2]] or "-",
                   ["<%s> in '%s'" % (c, tb[i]) for i, c in bb[j1:j2]] or "-"))

    print("headings       EN %3d / DE %3d" % (len(en["sections"]),
                                              len(de["sections"])))
    print("tagged divs    EN %3d / DE %3d" % (len(en["blocks"]),
                                              len(de["blocks"])))
    print("tagged spans   EN %3d / DE %3d" % (len(en["spans"]),
                                              len(de["spans"])))
    print("marked rows    EN %3d / DE %3d" % (len(en["rows"]),
                                              len(de["rows"])))
    print("note-type divs EN %3d / DE %3d" % (len(ba), len(bb)))
    print()
    for key in sorted(set(tagmap.counts(en)) | set(tagmap.counts(de))):
        print("  {.%-22s} EN %2d / DE %2d"
              % (key.replace(" ", " ."), tagmap.counts(en).get(key, 0),
                 tagmap.counts(de).get(key, 0)))

    if problems:
        print("\nTAGGING PROBLEMS (%d) -- these must be fixed:\n" % len(problems))
        for p in problems:
            print("  ! %s" % p)
    else:
        print("\nTagging is parallel: same tags, same order, same sections.")

    if divergences:
        print("\nCONTENT DIVERGENCES (%d) -- a passage exists in one language\n"
              "and not the other. Reported, not papered over (briefing 6);\n"
              "fixing them is an editorial decision, not a tagging one:\n"
              % len(divergences))
        for d in divergences:
            print("  ? %s" % d)

    return 1 if (problems or divergences) else 0


if __name__ == "__main__":
    sys.exit(main())
