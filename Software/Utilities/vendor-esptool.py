#!/usr/bin/env python3
"""
vendor-esptool.py — refresh the copy of esptool-js embedded in m32_installer.html.

The installer is one self-contained file: you can save it, carry it to a club
meeting with no internet and still flash a Morserino from a local copy of the
firmware. That means the flashing engine has to live inside the HTML, and it
also means nobody should ever hand-edit it there. Run this instead.

What it does:

  1. downloads esptool-js <VERSION> from the npm registry,
  2. checks the bundle against the SHA-256 pinned below (a mismatch is fatal —
     this is the only thing standing between us and a swapped-out flasher),
  3. rewrites the bundle's single trailing `export{...}` into an assignment to
     `window.esptoolJS` (the bundle has no imports, so that one substitution
     turns a module into a plain script),
  4. splices it between the BEGIN/END markers in m32_installer.html.

Bumping the version: change VERSION, run with --print-hash, paste the printed
digest into SHA256, run again. Then re-test flashing on both variants — this
is the code that writes to people's devices.

Usage:
  python3 vendor-esptool.py [--print-hash] [--installer PATH]
"""

from __future__ import annotations

import argparse
import hashlib
import io
import re
import sys
import tarfile
import urllib.request
from pathlib import Path

VERSION = "0.6.1"
SHA256 = "ef7d5a237d3f273ecf546bcee65dddad90bd82cf02f22a980d1537e0cd79a152"

TARBALL = f"https://registry.npmjs.org/esptool-js/-/esptool-js-{VERSION}.tgz"
MEMBER = "package/bundle.js"

BEGIN = "<!-- ==== BEGIN VENDORED esptool-js ==== -->"
END = "<!-- ==== END VENDORED esptool-js ==== -->"

# `export{Fe as ClassicReset,Ai as ESPLoader,...};` -> `{ClassicReset:Fe,ESPLoader:Ai,...}`
EXPORT_RE = re.compile(r"export\{([^}]*)\};?\s*$")


def fetch_bundle() -> bytes:
    with urllib.request.urlopen(TARBALL, timeout=120) as resp:
        payload = resp.read()
    with tarfile.open(fileobj=io.BytesIO(payload), mode="r:gz") as tar:
        member = tar.extractfile(MEMBER)
        if member is None:
            raise RuntimeError(f"{MEMBER} not found in {TARBALL}")
        return member.read()


def to_classic_script(bundle: str) -> str:
    """Turn the ESM bundle into a classic script exposing window.esptoolJS."""
    match = EXPORT_RE.search(bundle)
    if not match:
        raise RuntimeError(
            "no trailing `export{...}` found — the bundle format changed; "
            "re-read it before trusting this script"
        )
    if bundle.count("export{") != 1 or "import{" in bundle or "import(" in bundle:
        raise RuntimeError(
            "the bundle is no longer a single-export, import-free module; "
            "the classic-script conversion is not safe any more"
        )
    pairs = []
    for item in match.group(1).split(","):
        local, _, exported = item.partition(" as ")
        local, exported = local.strip(), (exported.strip() or local.strip())
        pairs.append(f"{exported}:{local}")
    return bundle[: match.start()] + "window.esptoolJS={" + ",".join(pairs) + "};"


def splice(installer: Path, script: str) -> None:
    html = installer.read_text(encoding="utf-8")
    start, end = html.find(BEGIN), html.find(END)
    if start < 0 or end < 0 or end < start:
        raise RuntimeError(f"BEGIN/END markers not found in {installer}")
    block = (
        f"{BEGIN}\n"
        f"<!-- esptool-js {VERSION}, Apache-2.0, verbatim except for the export\n"
        f"     statement. Do not edit: run vendor-esptool.py. -->\n"
        f"<script>{script}</script>\n"
    )
    installer.write_text(html[:start] + block + html[end:], encoding="utf-8")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument(
        "--installer",
        type=Path,
        default=Path(__file__).with_name("m32_installer.html"),
        help="Path to m32_installer.html",
    )
    ap.add_argument(
        "--print-hash",
        action="store_true",
        help="Download, print the SHA-256 and exit — for bumping VERSION.",
    )
    args = ap.parse_args(argv)

    print(f"downloading esptool-js {VERSION} …", file=sys.stderr)
    bundle = fetch_bundle()
    digest = hashlib.sha256(bundle).hexdigest()

    if args.print_hash:
        print(digest)
        return 0

    if digest != SHA256:
        print(
            f"SHA-256 mismatch for esptool-js {VERSION}:\n"
            f"  expected {SHA256}\n"
            f"  got      {digest}\n"
            f"Refusing to vendor. If you meant to bump the version, run with "
            f"--print-hash and update SHA256.",
            file=sys.stderr,
        )
        return 1

    script = to_classic_script(bundle.decode("utf-8"))
    splice(args.installer, script)
    print(
        f"vendored esptool-js {VERSION} ({len(script):,} bytes) into {args.installer}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
