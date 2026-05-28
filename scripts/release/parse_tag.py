#!/usr/bin/env python3
"""
parse_tag.py — parse a Morserino-32 release tag and emit structured info.

Tag scheme (from RELEASE_AUTOMATION_DESIGN.md §4):
  Stable:  V<major>.<minor>[.<patch>]               e.g. V8.1, V8.2.1
  Beta:    V<major>.<minor>[.<patch>]-beta.<N>      e.g. V8.1-beta.1, V8.2.1-beta.7

The counter <N> on beta tags is solely a tag-uniqueness device — the build date
(today's date when the workflow runs) is what gets stamped into filenames and
manifest entries. See §5 for the same-day-collision handling.

Output: JSON on stdout. Exit non-zero with a stderr message on parse failure.

Usage:
  parse_tag.py V8.1
  parse_tag.py V8.1-beta.3
  parse_tag.py V8.1 --date 260601    # override date (for testing)

Example output:
  {"channel":"stable","version":"8.1","major":"8","counter":null,"date":"260601"}
  {"channel":"beta","version":"8.1","major":"8","counter":3,"date":"260601"}
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import sys

STABLE_RE = re.compile(r"^V(\d+\.\d+(?:\.\d+)?)$")
BETA_RE = re.compile(r"^V(\d+\.\d+(?:\.\d+)?)-beta\.(\d+)$")


def parse(tag: str, date: str) -> dict:
    m = STABLE_RE.match(tag)
    if m:
        version = m.group(1)
        return {
            "channel": "stable",
            "version": version,
            "major": version.split(".", 1)[0],
            "counter": None,
            "date": date,
        }
    m = BETA_RE.match(tag)
    if m:
        version = m.group(1)
        return {
            "channel": "beta",
            "version": version,
            "major": version.split(".", 1)[0],
            "counter": int(m.group(2)),
            "date": date,
        }
    raise ValueError(
        f"Tag {tag!r} matches neither the stable pattern "
        f"(V<major>.<minor>[.<patch>]) nor the beta pattern "
        f"(V<major>.<minor>[.<patch>]-beta.<N>)."
    )


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("tag", help="The git tag to parse, e.g. V8.1 or V8.1-beta.3")
    ap.add_argument(
        "--date",
        help="Override the build date (YYMMDD format). "
        "Default: today in the runner's local timezone.",
    )
    args = ap.parse_args(argv)

    date = args.date or dt.date.today().strftime("%y%m%d")
    if not re.fullmatch(r"\d{6}", date):
        print(f"--date must be YYMMDD (6 digits), got {date!r}", file=sys.stderr)
        return 2

    try:
        info = parse(args.tag, date)
    except ValueError as e:
        print(str(e), file=sys.stderr)
        return 1

    json.dump(info, sys.stdout, separators=(",", ":"))
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
