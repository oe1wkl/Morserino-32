#!/usr/bin/env python3
"""
update_manifest.py — prepend a new entry to a Dropbox-hosted versions.json.

The manifest is an array, newest first (RELEASE_AUTOMATION_DESIGN.md §9).
This script:
  - reads the existing manifest (if any),
  - fails fast if an entry with the same `filename` already exists
    (refuses to overwrite),
  - constructs a new entry,
  - prepends it,
  - writes atomically via <manifest>.tmp + rename.

Existing entries are preserved verbatim. Historical typos/duplicates stay
as they are; the developer can fix them by hand.

Usage:
  update_manifest.py \\
      --manifest /path/to/firmware/versions.json \\
      --version "8.1" \\
      --filename "fw_m32_V8.1.bin" \\
      --channel stable \\
      [--notes "custom note"] \\
      [--dry-run]

  update_manifest.py \\
      --manifest /path/to/firmware/m32p/versions.json \\
      --version "8.1 beta 260601 #2" \\
      --filename "fw_m32p_V8.1_beta_260601.2.bin" \\
      --channel beta \\
      [--notes "custom note"]

If --notes is omitted, a templated default is used:
  stable: "Release of V.<ver-without-suffix>"
  beta:   "Beta of V.<base-ver> dated <YYMMDD>[ #N]"

The "version string" passed in is whatever the rename_binaries.py step
produced; this script just stores it verbatim as the entry's `version`
field. The `notes` default is derived from that string when not overridden.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path


_BETA_VERSION_RE = re.compile(r"^(\S+) beta (\d{6})(?: #(\d+))?$")


def default_notes(channel: str, manifest_version: str) -> str:
    """Templated default — overridden by --notes when supplied."""
    if channel == "stable":
        return f"Release of V.{manifest_version}"
    # Beta: parse the structured version string we got from rename_binaries.py.
    m = _BETA_VERSION_RE.match(manifest_version)
    if m:
        base, date, counter = m.group(1), m.group(2), m.group(3)
        if counter:
            return f"Beta of V.{base} dated {date} #{counter}"
        return f"Beta of V.{base} dated {date}"
    # Fallback (shouldn't happen): use the version string as-is.
    return f"Beta: {manifest_version}"


def build_entry(
    channel: str, manifest_version: str, filename: str, notes: str | None
) -> dict:
    entry: dict = {
        "version": manifest_version,
        "filename": filename,
        "notes": notes if notes is not None else default_notes(channel, manifest_version),
        "beta": channel == "beta",
    }
    return entry


def update(
    manifest_path: Path,
    entry: dict,
    dry_run: bool,
    log,
) -> None:
    # Load existing
    if manifest_path.is_file():
        try:
            existing = json.loads(manifest_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            raise RuntimeError(f"Could not parse existing manifest: {e}") from e
        if not isinstance(existing, list):
            raise RuntimeError(
                f"Expected a JSON array at top of {manifest_path}, got "
                f"{type(existing).__name__}"
            )
    else:
        log(f"manifest does not exist yet — creating fresh: {manifest_path}")
        existing = []

    # Refuse to add a duplicate filename.
    for e in existing:
        if e.get("filename") == entry["filename"]:
            raise RuntimeError(
                f"An entry with filename {entry['filename']!r} already exists "
                f"in {manifest_path}. Refusing to overwrite. Bump the date or "
                f"version, or remove the existing entry by hand."
            )

    new_list = [entry] + existing
    log(f"prepending entry: {json.dumps(entry, ensure_ascii=False)}")

    if dry_run:
        log(f"would write {len(new_list)} entries to {manifest_path}")
        return

    # Atomic write: tmp file in the same directory, then rename.
    tmp_path = manifest_path.with_suffix(manifest_path.suffix + ".tmp")
    # Match existing formatting: 2-space indent, no trailing newline on last line.
    serialized = json.dumps(new_list, indent=2, ensure_ascii=False)
    tmp_path.write_text(serialized + "\n", encoding="utf-8")
    os.replace(tmp_path, manifest_path)
    log(f"wrote {len(new_list)} entries to {manifest_path}")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--manifest", required=True, type=Path,
                    help="Path to versions.json (per platform).")
    ap.add_argument("--version", required=True,
                    help="The 'version' string to put in the entry, e.g. "
                         "'8.1' or '8.1 beta 260601' or '8.1 beta 260601 #2'.")
    ap.add_argument("--filename", required=True,
                    help="The 'filename' to put in the entry.")
    ap.add_argument("--channel", required=True, choices=["stable", "beta"])
    ap.add_argument("--notes", default=None,
                    help="Override the templated 'notes' field. If empty "
                         "string is passed, an empty notes field is stored.")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args(argv)

    if not args.manifest.parent.is_dir():
        print(
            f"Manifest's parent directory does not exist: {args.manifest.parent}",
            file=sys.stderr,
        )
        return 2

    def log(msg: str) -> None:
        prefix = "[dry-run] " if args.dry_run else ""
        print(prefix + msg, file=sys.stderr)

    entry = build_entry(args.channel, args.version, args.filename, args.notes)
    try:
        update(args.manifest, entry, args.dry_run, log)
    except RuntimeError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    json.dump(entry, sys.stdout, ensure_ascii=False)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
