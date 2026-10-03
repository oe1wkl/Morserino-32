#!/usr/bin/env python3
"""
check_doc_pdfs.py -- were the committed FAQ and protocol PDFs built from their sources as they are now?

The same gate as "User Manual/check_manual_fresh.py", for the two other documents that are generated
but committed (TODO E2): the M32 Protocol description and the M32 Pocket FAQ (EN + DE). Both used to
be hand exports and drifted for months; on 2026-10-02 the protocol Markdown gained two sections and
its PDF did not.

The fingerprint travels INSIDE the PDF: build.sh asks this script for it and hands it to pandoc as
the document keywords, which weasyprint writes into the PDF's metadata ("docsrc:<12 hex>"). This
script inflates the PDF's compressed streams (zlib, no PDF library needed) to read it back, and
compares it with a fingerprint of the sources in the tree. Rebuilding in CI and diffing would not
work: PDFs are not reproducible byte for byte across tool versions.

Usage:
  check_doc_pdfs.py --check               verify every document (this is what CI runs)
  check_doc_pdfs.py --fingerprint <doc>   print a document's source fingerprint (build.sh uses this)
"""
import argparse
import hashlib
import os
import re
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))

# id: (folder, sources, pdf, how to rebuild). The sources are everything that changes the output;
# build.sh itself is left out, as in the manual gate (a comment edit must not demand a rebuild).
DOCS = {
    "protocol": ("Protocol Description", ["M32 Protocol.md", "style.css"],
                 "M32 Protocol.pdf", "./build.sh"),
    "faq-en":   ("FAQ", ["Morserino-32 Pocket FAQ.md", "style.css"],
                 "Morserino-32 Pocket FAQ.pdf", "./build.sh en"),
    "faq-de":   ("FAQ", ["Morserino-32 Pocket FAQ (Deutsch).md", "style.css"],
                 "Morserino-32 Pocket FAQ (Deutsch).pdf", "./build.sh de"),
}
STAMP_RE = re.compile(rb"docsrc:([0-9a-f]{12})")


def fingerprint(doc):
    """Hash of a document's sources; line endings normalised, nothing else (Markdown is whitespace-sensitive)."""
    folder, sources, _pdf, _how = DOCS[doc]
    h = hashlib.sha256()
    for name in sources:
        path = os.path.join(HERE, folder, name)
        h.update(name.encode("utf-8") + b"\0")
        if os.path.exists(path):
            with open(path, "rb") as fh:
                h.update(fh.read().replace(b"\r\n", b"\n"))
        else:
            h.update(b"<missing>")
        h.update(b"\0")
    return h.hexdigest()[:12]


def read_stamp(pdf_path):
    """The docsrc: keyword from the PDF's metadata, wherever weasyprint put it (plain or in a Flate stream)."""
    if not os.path.exists(pdf_path):
        return None
    with open(pdf_path, "rb") as fh:
        data = fh.read()
    m = STAMP_RE.search(data)
    if m:
        return m.group(1).decode()
    for s in re.finditer(rb"stream\r?\n(.*?)\r?\nendstream", data, re.S):
        try:
            m = STAMP_RE.search(zlib.decompress(s.group(1)))
        except zlib.error:
            continue
        if m:
            return m.group(1).decode()
    return None


def check():
    failures = 0
    for doc, (folder, _sources, pdf, how) in DOCS.items():
        want, have = fingerprint(doc), read_stamp(os.path.join(HERE, folder, pdf))
        if have == want:
            continue
        failures += 1
        if have is None:
            print(f"FAIL: {folder}/{pdf} carries no source stamp - it predates this check, or was not made by build.sh.")
        else:
            print(f"FAIL: {folder}/{pdf} is older than its sources.")
            print(f"      sources now: {want}, built from: {have}")
        print(f"  Rebuild with:  (cd 'Documentation/{folder}' && {how})")
    if failures:
        return 1
    print("OK: the FAQ and protocol PDFs match their sources (%s)." % ", ".join(DOCS))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--check", action="store_true")
    g.add_argument("--fingerprint", choices=sorted(DOCS))
    args = ap.parse_args()
    if args.fingerprint:
        print(fingerprint(args.fingerprint))
        return 0
    return check()


if __name__ == "__main__":
    sys.exit(main())
