#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
tagmap.py -- read the variant tags out of a tagged manual source.

Shared by check_parallelism.py and the inventory generators. Nothing here
writes; it only parses manual_<lang>.md into:

  * sections   -- the heading tree, in document order
  * blocks     -- every fenced div that carries a variant class
  * spans      -- every bracketed span that carries a variant class
  * rows       -- every table row marked with the row-marker convention
                  (an empty span at the very start of the row's first cell)

Vocabulary (briefing 3): classes are exactly classic, pocket, pocket-a11y.
Untagged means "all variants". pocket-a11y is not implied by pocket.
"""
import os
import re

VARIANTS = ("classic", "pocket", "pocket-a11y")

MANUAL = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "Documentation", "User Manual", "Version 9.x"))

# ::: {.pocket .pocket-a11y}  /  ::: {.note .classic}  /  ::: note  /  :::
DIV_OPEN = re.compile(r"^(:{3,})\s*(\{[^}]*\}|\S+)\s*$")
DIV_CLOSE = re.compile(r"^:{3,}\s*$")
# [text]{.classic}  -- text may span lines, so spans are matched on joined text
SPAN = re.compile(r"\[((?:[^\[\]]|\[[^\]]*\])*)\]\{([^}]*)\}", re.S)
# row marker: pipe, optional space, empty span with classes, then the cell text
ROW_MARK = re.compile(r"^\|\s*\[\]\{([^}]*)\}")


def _classes(attr):
    """Class names from a fenced-div attribute blob or a span's {...} body.

    Handles both forms the manual uses: the shorthand `::: note`, where the
    bare word is the class, and `::: {.note .pocket}`.
    """
    if not attr.startswith("{"):
        return [attr] if attr else []
    return [c[1:] for c in attr[1:-1].split() if c.startswith(".")]


def variant_classes(attr):
    return [c for c in _classes(attr) if c in VARIANTS]


def unknown_classes(attr):
    """Classes that are neither a variant key nor one of the manual's own."""
    known = set(VARIANTS) | {"note", "important", "warning", "quote", "unnumbered"}
    return [c for c in _classes(attr) if c not in known]


def parse(lang):
    path = os.path.join(MANUAL, "manual_%s.md" % lang)
    lines = open(path, encoding="utf-8").read().split("\n")

    sections, blocks, spans, rows = [], [], [], []
    stack = []          # open fenced divs: (line, classes, variant_classes)
    section = None
    fenced = False      # inside a ``` code fence

    for n, line in enumerate(lines, 1):
        if line.startswith("```"):
            fenced = not fenced
            continue
        if fenced:
            continue

        h = re.match(r"^(#{1,6}) +(.*?)\s*(\{[^}]*\})?\s*$", line)
        if h:
            section = {"level": len(h.group(1)), "title": h.group(2).strip(),
                       "line": n, "tags": []}
            sections.append(section)

        m = DIV_OPEN.match(line)
        if m and not DIV_CLOSE.match(line):
            attr = m.group(2)
            v = variant_classes(attr)
            stack.append((n, attr, v))
            if v:
                rec = {"line": n, "classes": v, "all_classes": _classes(attr),
                       "section": section["title"] if section else "(front matter)",
                       "end": None}
                blocks.append(rec)
                if section:
                    section["tags"].append(("div", tuple(v), n))
            continue
        if DIV_CLOSE.match(line):
            if stack:
                start, attr, v = stack.pop()
                if v:
                    for rec in reversed(blocks):
                        if rec["line"] == start:
                            rec["end"] = n
                            break
            continue

        r = ROW_MARK.match(line)
        if r:
            v = variant_classes("{%s}" % r.group(1))
            if v:
                rows.append({"line": n, "classes": v,
                             "section": section["title"] if section else "?",
                             "text": line.strip()})
                if section:
                    section["tags"].append(("row", tuple(v), n))

    # spans: match over the whole text so a span may cross source lines
    text = "\n".join(lines)
    offsets, pos = [], 0
    for line in lines:
        offsets.append(pos)
        pos += len(line) + 1

    def line_of(idx):
        lo, hi = 0, len(offsets) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if offsets[mid] <= idx:
                lo = mid
            else:
                hi = mid - 1
        return lo + 1

    for m in SPAN.finditer(text):
        v = variant_classes("{%s}" % m.group(2))
        if not v:
            continue
        n = line_of(m.start())
        body = re.sub(r"\s+", " ", m.group(1)).strip()
        if not body:
            continue        # empty span == row marker, already collected
        spans.append({"line": n, "classes": v, "text": body})

    for s in spans:
        sec = None
        for candidate in sections:
            if candidate["line"] <= s["line"]:
                sec = candidate
            else:
                break
        s["section"] = sec["title"] if sec else "(front matter)"
        if sec:
            sec["tags"].append(("span", tuple(s["classes"]), s["line"]))

    for sec in sections:
        sec["tags"].sort(key=lambda t: t[2])

    return {"lang": lang, "path": path, "lines": lines, "sections": sections,
            "blocks": blocks, "spans": spans, "rows": rows,
            "unclosed": stack}


def counts(tm):
    out = {}
    for kind in ("blocks", "spans", "rows"):
        for rec in tm[kind]:
            out[" ".join(rec["classes"])] = out.get(" ".join(rec["classes"]), 0) + 1
    return out


if __name__ == "__main__":
    import sys
    for lang in (sys.argv[1:] or ["en", "de"]):
        tm = parse(lang)
        print("=" * 60)
        print("%s: %d tagged divs, %d tagged spans, %d marked table rows"
              % (tm["path"].split("/")[-1], len(tm["blocks"]), len(tm["spans"]),
                 len(tm["rows"])))
        if tm["unclosed"]:
            print("  !! unclosed fenced divs opened at lines:",
                  [s[0] for s in tm["unclosed"]])
        for key, n in sorted(counts(tm).items()):
            print("   %-24s %d" % ("{." + key.replace(" ", " .") + "}", n))
