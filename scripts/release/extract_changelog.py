#!/usr/bin/env python3
"""
extract_changelog.py — pull the changelog section for a given version
from Software/README.md.

Section headings in that file are inconsistent: '### Changes V. 8.0',
'### Changes V.7.1', '### Changes V.7.0'. The match is tolerant of the
extra space and of the dot/space between 'V' and the digits.

Exits non-zero if no matching section is found — the release workflow
relies on this to fail fast before any building happens (§7).

Usage:
  extract_changelog.py --version 8.1 [--readme PATH]

Prints the captured block (minus the heading line) to stdout.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

DEFAULT_README = Path(__file__).resolve().parents[2] / "Software" / "README.md"


def _heading_regex(version: str) -> re.Pattern:
    """Build a tolerant regex for the '### Changes V<ver>' heading.

    Tolerates:
      - case ('Changes' / 'CHANGES' / 'changes')
      - optional whitespace after 'V' and between the 'V'/'.' and the number
        ('V. 8.0', 'V.8.0', 'V 8.0', 'V8.0')
    """
    escaped = re.escape(version)
    return re.compile(
        r"^###\s+Changes\s+V\.?\s*" + escaped + r"\s*$",
        re.MULTILINE | re.IGNORECASE,
    )


def extract(readme_text: str, version: str) -> str:
    pattern = _heading_regex(version)
    match = pattern.search(readme_text)
    if not match:
        raise LookupError(
            f"No '### Changes V{version}' section found in the README. "
            f"Update Software/README.md and re-tag."
        )

    body_start = match.end()
    # Find the next '### ' heading after this one (or use EOF).
    next_heading = re.search(r"^###\s", readme_text[body_start:], re.MULTILINE)
    if next_heading:
        body_end = body_start + next_heading.start()
    else:
        body_end = len(readme_text)

    body = readme_text[body_start:body_end].strip("\n")
    return body


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument(
        "--version",
        required=True,
        help="Version string to look up (e.g. 8.1 or 8.2.1).",
    )
    ap.add_argument(
        "--readme",
        type=Path,
        default=DEFAULT_README,
        help=f"Path to README.md (default: {DEFAULT_README}).",
    )
    args = ap.parse_args(argv)

    if not args.readme.is_file():
        print(f"README not found: {args.readme}", file=sys.stderr)
        return 2

    text = args.readme.read_text(encoding="utf-8")
    try:
        body = extract(text, args.version)
    except LookupError as e:
        print(str(e), file=sys.stderr)
        return 1

    if not body.strip():
        # Soft warning per §7 — the workflow may substitute a placeholder.
        print(
            f"Warning: '### Changes V{args.version}' section is empty.",
            file=sys.stderr,
        )

    sys.stdout.write(body)
    if not body.endswith("\n"):
        sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
