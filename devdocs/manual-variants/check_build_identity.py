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
  2. the HTML's visible text is byte-identical once tags are stripped.

Usage (from anywhere):
    python3 check_build_identity.py en [reference.pdf]
    python3 check_build_identity.py de [reference.pdf]

With no reference given, the committed m32UserManual_v9_<lang>.pdf is used --
which is the right reference only as long as the tagging pass has not rebuilt
it. Build artefacts are written to a temp dir and removed again; the committed
manual_<lang>.html / .pdf are never touched.
"""
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
    """Run the same pandoc + weasyprint invocation build.sh uses."""
    src = os.path.join(MANUAL, "manual_%s.md" % lang)
    tmp_html = os.path.join(MANUAL, "_identitycheck_%s.html" % lang)
    pdf = os.path.join(outdir, "check_%s.pdf" % lang)

    cmd = ["pandoc", src, "-o", tmp_html, "--standalone", "--toc",
           "--toc-depth=3", "--number-sections", "--css=style.css",
           "--resource-path=.:images",
           "--metadata", "title=%s" % ("Morserino-32 Benutzerhandbuch"
                                       if lang == "de"
                                       else "Morserino-32 User Manual"),
           "--metadata", "lang=%s" % lang,
           "--wrap=none", "--from", "markdown+fenced_divs",
           "--include-before-body=%s" % ("title_de.html" if lang == "de"
                                         else "title.html")]
    if lang == "de":
        cmd.append("--lua-filter=normalize_ids.lua")
    subprocess.run(cmd, cwd=MANUAL, check=True)
    subprocess.run(["weasyprint", tmp_html, pdf], cwd=MANUAL, check=True)
    with open(tmp_html, encoding="utf-8") as fh:
        markup = fh.read()
    os.remove(tmp_html)
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

        print("language      : %s" % lang)
        print("reference     : %s" % os.path.relpath(ref, MANUAL))
        print("pages         : %d (reference %d)" % (len(new_pages),
                                                     len(ref_pages)))
        print("html text     : %d characters" % len(html_text))
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
