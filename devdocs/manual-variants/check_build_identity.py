#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_build_identity.py -- prove that variant tagging changed nothing visible.

With no filter in place, pandoc renders fenced divs and bracketed spans
transparently, so the built PDF must be indistinguishable from the one built
before the tags went in. That is the acceptance test for the tagging pass.

The generated HTML is *not* byte-identical -- pandoc emits the new <div>/<span>
wrappers, and promotes a div whose first block is a heading into a <section>
that carries the heading's id. So this script checks the two things that
actually matter:

  1. the rendered PDF has the same number of pages, and every page carries the
     same text as the reference PDF (nothing reflowed across a page boundary);
  2. every H1/H2/H3 in the source reaches the table of contents.

Check 2 exists because of a trap that fails *silently*: a fenced div wrapping
two or more sibling sections drops their headings from the TOC (pandoc promotes
a div to <section> only when it holds exactly one section; otherwise the
headings inside it never become sections and the TOC skips them). Numbering
still looks right, pandoc says nothing, and the manual ships with holes in its
contents list. Rule: one div wraps at most one section.

Usage (from anywhere):
    python3 check_build_identity.py en [reference.pdf]
    python3 check_build_identity.py de [reference.pdf]

With no reference given, the committed m32UserManual_v9_<lang>.pdf is used --
which is the right reference only as long as the tagging pass has not rebuilt
it. Build artefacts are written to a temp dir and removed again; the committed
manual_<lang>.html / .pdf are never touched.
"""
import html
import html.parser
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
MANUAL = os.path.normpath(os.path.join(
    HERE, "..", "..", "Documentation", "User Manual", "Version 9.x"))


def build(lang, outdir):
    """Build via the manual's own build.sh, in a throwaway copy of the sources.

    Calling build.sh rather than reimplementing its pandoc invocation matters:
    an earlier version of this script duplicated the command line, and when the
    title pages became templates (@MAJOR@, @MONTH_YEAR@) the copy kept passing
    the raw template to --include-before-body and "verified" a manual whose
    title page read 'Version @MAJOR@.x'. One source of truth for the build.

    The copy is so that the real manual_<lang>.html and the committed PDFs are
    never touched. The temp directory keeps the 'Version <major>.x' name
    because build.sh derives the major version, and so the PDF filename, from
    it.
    """
    workdir = os.path.join(outdir, os.path.basename(MANUAL))
    os.makedirs(workdir, exist_ok=True)
    for name in ("manual_en.md", "manual_de.md", "title.html", "title_de.html",
                 "style.css", "normalize_ids.lua", "build.sh"):
        src = os.path.join(MANUAL, name)
        if os.path.exists(src):
            shutil.copy2(src, workdir)
    images = os.path.join(MANUAL, "images")
    if os.path.isdir(images):
        shutil.copytree(images, os.path.join(workdir, "images"),
                        dirs_exist_ok=True)
    os.chmod(os.path.join(workdir, "build.sh"), 0o755)

    subprocess.run(["./build.sh", lang], cwd=workdir, check=True,
                   stdout=subprocess.DEVNULL)

    major = re.search(r"Version (\d+)\.x", os.path.basename(MANUAL)).group(1)
    pdf = os.path.join(workdir, "m32UserManual_v%s_%s.pdf" % (major, lang))
    if not os.path.exists(pdf):
        raise SystemExit("build.sh produced no %s" % os.path.basename(pdf))
    with open(os.path.join(workdir, "manual_%s.html" % lang),
              encoding="utf-8") as fh:
        markup = fh.read()
    return markup, pdf


class TextOnly(html.parser.HTMLParser):
    """Visible text of the document, tags and attributes discarded."""
    SKIP = {"script", "style"}

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.out = []
        self._skip = 0

    def handle_starttag(self, tag, attrs):
        if tag in self.SKIP:
            self._skip += 1

    def handle_endtag(self, tag):
        if tag in self.SKIP and self._skip:
            self._skip -= 1

    def handle_data(self, data):
        if not self._skip:
            self.out.append(data)

    def text(self):
        return re.sub(r"\s+", " ", "".join(self.out)).strip()


def page_texts(path):
    from pypdf import PdfReader
    return [re.sub(r"\s+", " ", p.extract_text() or "").strip()
            for p in PdfReader(path).pages]


def toc_gaps(lang, markup):
    """Headings down to --toc-depth=3 that never made it into the TOC."""
    src = os.path.join(MANUAL, "manual_%s.md" % lang)
    wanted, fenced = [], False
    for line in open(src, encoding="utf-8"):
        if line.startswith("```"):
            fenced = not fenced
        m = re.match(r"^(#{1,3}) +(.*?)\s*(\{[^}]*\})?\s*$", line)
        if m and not fenced:
            wanted.append(m.group(2).strip())
    toc = re.search(r'<nav id="TOC".*?</nav>', markup, re.S)
    listed = (re.sub(r"\s+", " ",
                     html.unescape(re.sub(r"<[^>]+>", "", toc.group(0))))
              if toc else "")
    # TOC text carries the section number ahead of the title; a title is present
    # when its own words appear in order, so compare on the stripped title alone.
    return [h for h in wanted
            if re.sub(r"\s+", " ", re.sub(r"[*_`\\]", "", h)) not in listed]


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("en", "de"):
        sys.exit(__doc__)
    lang = sys.argv[1]
    ref = (sys.argv[2] if len(sys.argv) > 2
           else os.path.join(MANUAL, "m32UserManual_v9_%s.pdf" % lang))
    if not os.path.exists(ref):
        sys.exit("reference PDF not found: %s" % ref)

    outdir = tempfile.mkdtemp(prefix="m32identity-")
    try:
        markup, pdf = build(lang, outdir)

        ref_pages, new_pages = page_texts(ref), page_texts(pdf)
        problems = []
        if len(ref_pages) != len(new_pages):
            problems.append("page count %d -> %d" % (len(ref_pages),
                                                     len(new_pages)))
        for n, (a, b) in enumerate(zip(ref_pages, new_pages), 1):
            if a != b:
                problems.append("page %d text differs" % n)

        stripped = TextOnly()
        stripped.feed(markup)
        html_text = stripped.text()

        missing = toc_gaps(lang, markup)
        for h in missing:
            problems.append("heading missing from TOC: %s" % h)

        print("language      : %s" % lang)
        print("reference     : %s" % os.path.relpath(ref, MANUAL))
        print("pages         : %d (reference %d)" % (len(new_pages),
                                                     len(ref_pages)))
        print("html text     : %d characters" % len(html_text))
        print("toc           : %s" % ("complete" if not missing
                                      else "%d heading(s) MISSING" % len(missing)))
        if problems:
            print("\nDIFFERENCES (%d):" % len(problems))
            for p in problems[:40]:
                print("  ! %s" % p)
            for n, (a, b) in enumerate(zip(ref_pages, new_pages), 1):
                if a != b:
                    print("\nfirst differing page %d" % n)
                    print("  reference: %s" % a[:300])
                    print("  built    : %s" % b[:300])
                    break
            return 1
        print("\nOK - %d pages, identical text on every page." % len(new_pages))
        return 0
    finally:
        shutil.rmtree(outdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
