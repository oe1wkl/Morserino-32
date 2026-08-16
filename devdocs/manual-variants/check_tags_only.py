#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_tags_only.py -- prove the tagging pass changed nothing but tags.

The strongest guarantee available: strip every variant tag back out of the
tagged source and compare the result with the source as it was before tagging.
If they are identical, no prose was reworded, no line was reflowed and nothing
was lost -- whatever the diff looks like.

Stripping undoes exactly what tagging did:
  * a fenced div whose classes are *only* variant keys      -> removed entirely
  * a div that also carries note/important/warning/quote    -> restored to the
                                                               `::: note` shorthand
  * an empty row-marker span `[]{.classic}`                 -> removed
  * a bracketed span `[text]{.pocket}`                      -> unwrapped to text

Usage:
    python3 check_tags_only.py [git-ref]      # default: master

The reference is a git ref holding the pre-tagging sources, so this keeps
working after the branch is merged (point it at the merge base).
"""
import difflib
import os
import re
import subprocess
import sys

REL = "Documentation/User Manual/Version 9.x/manual_%s.md"
REPO = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", ".."))
VARIANTS = ("classic", "pocket", "pocket-a11y")
BOXES = ("note", "important", "warning", "quote")


def strip_tags(text):
    out, stack = [], []
    for line in text.split("\n"):
        opener = re.match(r"^:{3,}\s*(\{[^}]*\}|\S+)\s*$", line)
        closer = re.fullmatch(r":{3,}\s*", line)
        if opener and not closer:
            attr = opener.group(1)
            cls = ([c[1:] for c in attr.strip("{}").split() if c.startswith(".")]
                   if attr.startswith("{") else [attr])
            variant = [c for c in cls if c in VARIANTS]
            other = [c for c in cls if c not in VARIANTS]
            if variant and not other:
                stack.append("drop")
                continue
            stack.append("keep")
            out.append("::: " + other[0] if variant and len(other) == 1 else line)
            continue
        if closer:
            if stack and stack.pop() == "drop":
                continue
            out.append(line)
            continue
        out.append(line)
    s = "\n".join(out)
    s = re.sub(r"\[\]\{[^}]*\}", "", s)                       # row markers
    s = re.sub(r"\[((?:[^\[\]]|\[[^\]]*\])*)\]\{\s*\.(?:%s)[^}]*\}"
               % "|".join(VARIANTS), r"\1", s, flags=re.S)    # inline spans
    return s


def normalise(t):
    return re.sub(r"\n{3,}", "\n\n", t).strip()


def main():
    ref = sys.argv[1] if len(sys.argv) > 1 else "master"
    failed = False
    for lang in ("en", "de"):
        path = REL % lang
        tagged = open(os.path.join(REPO, path), encoding="utf-8").read()
        try:
            before = subprocess.run(["git", "show", "%s:%s" % (ref, path)],
                                    cwd=REPO, capture_output=True, text=True,
                                    check=True).stdout
        except subprocess.CalledProcessError:
            sys.exit("cannot read %s at ref '%s'" % (path, ref))
        a, b = normalise(before), normalise(strip_tags(tagged))
        if a == b:
            print("%s: tags removed -> identical to %s" % (lang, ref))
        else:
            failed = True
            print("%s: DIFFERS from %s once tags are removed" % (lang, ref))
            for line in list(difflib.unified_diff(
                    a.split("\n"), b.split("\n"), ref, "stripped",
                    lineterm="", n=1))[:40]:
                print("  " + line)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
