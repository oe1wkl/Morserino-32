#!/usr/bin/env python3
"""
rename_binaries.py — stage renamed firmware binaries for a release.

For each of the two release platforms (M32 original, M32 Pocket):
  1. Compute the target filename per RELEASE_AUTOMATION_DESIGN.md §5.
     For beta releases, check for same-day collisions in the Dropbox folder
     and in the existing manifest; if either is hit, append the tag's
     counter as a suffix (e.g. fw_m32_V8.1_beta_260601.2.bin).
  2. Copy the PlatformIO build output into the staging directory under
     the computed filename.
  3. Strip macOS extended attributes (xattr -c) so the file isn't flagged
     as quarantined when re-uploaded.

The same collision-detection logic is used to compute the manifest version
string ("8.1" for stable, "8.1 beta 260601" or "8.1 beta 260601 #2" for beta).
Both the filename and the manifest version are emitted as JSON on stdout so
update_manifest.py can consume them without re-deriving the logic.

Inputs:
  --channel        stable | beta
  --version        e.g. 8.1
  --date           YYMMDD (required for beta, ignored for stable)
  --counter        tag counter int (required for beta, ignored for stable)
  --build-root     parent of <env>/firmware.bin
                   (typically Software/src/.pio/build)
  --dropbox-root   for collision checks
                   (typically $MORSERINO_DROPBOX_ROOT)
  --staging-dir    where renamed copies are placed
  --dry-run        log what would happen; touch nothing

Exits non-zero on error.

Output (stdout, one JSON object):
  {
    "m32":  {"source": "...", "filename": "fw_m32_V8.1.bin",
             "staged_path": "...", "manifest_version": "8.1"},
    "m32p": {"source": "...", "filename": "fw_m32p_V8.1.bin",
             "staged_path": "...", "manifest_version": "8.1"}
  }
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

# Platform definitions: (pio_env_name, filename_prefix, dropbox_subdir)
PLATFORMS = {
    "m32":  ("heltec_wifi_lora_32_V2", "fw_m32",  ""),
    "m32p": ("pocketwroom",            "fw_m32p", "m32p"),
}


def compute_target(
    platform_key: str,
    channel: str,
    version: str,
    date: str | None,
    counter: int | None,
    dropbox_root: Path,
) -> tuple[str, str]:
    """Return (filename, manifest_version) for one platform, applying the
    same-day collision rule from §5.
    """
    _, prefix, subdir = PLATFORMS[platform_key]
    dropbox_dir = dropbox_root / subdir if subdir else dropbox_root
    manifest_path = dropbox_dir / "versions.json"

    if channel == "stable":
        filename = f"{prefix}_V{version}.bin"
        manifest_version = version
        # Stable: refuse to overwrite an existing file or manifest entry.
        # Different policy from beta — there's no "next beta" disambiguator
        # for stable; if V8.1 exists, the developer must bump or remove.
        if (dropbox_dir / filename).exists():
            raise RuntimeError(
                f"Stable target {filename} already exists in {dropbox_dir}. "
                f"Refusing to overwrite. Bump the version or remove the "
                f"existing file by hand."
            )
        if manifest_path.is_file():
            try:
                entries = json.loads(manifest_path.read_text(encoding="utf-8"))
            except json.JSONDecodeError as e:
                raise RuntimeError(
                    f"Could not parse {manifest_path} for collision check: {e}"
                ) from e
            for e in entries:
                if e.get("filename") == filename or e.get("version") == manifest_version:
                    raise RuntimeError(
                        f"Stable target {filename} (version {manifest_version!r}) "
                        f"already exists in {manifest_path}. Refusing to "
                        f"overwrite. Bump the version or remove the entry by hand."
                    )
        return filename, manifest_version

    # Beta: try the no-suffix form first
    base_filename = f"{prefix}_V{version}_beta_{date}.bin"
    base_manifest_version = f"{version} beta {date}"

    # Collision check: file exists in Dropbox OR manifest already has the
    # base manifest_version OR manifest already has the base filename.
    file_collision = (dropbox_dir / base_filename).exists()
    manifest_collision = False
    if manifest_path.is_file():
        try:
            entries = json.loads(manifest_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as e:
            raise RuntimeError(
                f"Could not parse {manifest_path} for collision check: {e}"
            ) from e
        for e in entries:
            if e.get("filename") == base_filename:
                manifest_collision = True
                break
            if e.get("version") == base_manifest_version:
                manifest_collision = True
                break

    if not (file_collision or manifest_collision):
        return base_filename, base_manifest_version

    if counter is None:
        raise RuntimeError(
            f"Same-day beta collision detected for {base_filename} but no "
            f"--counter was provided to disambiguate."
        )

    suffixed_filename = f"{prefix}_V{version}_beta_{date}.{counter}.bin"
    suffixed_version = f"{version} beta {date} #{counter}"

    # Defensive: the suffixed form must also not collide. If it does, the
    # developer pushed the same beta tag twice for some reason — bail rather
    # than silently overwrite.
    if (dropbox_dir / suffixed_filename).exists():
        raise RuntimeError(
            f"Even the suffixed filename {suffixed_filename} already exists "
            f"in {dropbox_dir}. Refusing to overwrite."
        )
    return suffixed_filename, suffixed_version


def stage_binary(
    src: Path, dst: Path, dry_run: bool, log
) -> None:
    """Copy src→dst and strip macOS xattrs from dst."""
    if not src.is_file():
        raise FileNotFoundError(f"Build output not found: {src}")

    log(f"copy {src} → {dst}")
    if not dry_run:
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

    log(f"xattr -c {dst}")
    if not dry_run:
        # xattr is available on every macOS; on non-macOS runners (e.g. for
        # local linting) this is a no-op via skip-on-missing.
        if shutil.which("xattr"):
            subprocess.run(
                ["xattr", "-c", str(dst)],
                check=True,
                stdout=subprocess.DEVNULL,
            )
        else:
            log("  (xattr not available — skipping; expected on macOS only)")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--channel", required=True, choices=["stable", "beta"])
    ap.add_argument("--version", required=True, help="e.g. 8.1 or 8.2.1")
    ap.add_argument("--date", help="YYMMDD (required for beta)")
    ap.add_argument("--counter", type=int, help="tag counter (required for beta)")
    ap.add_argument(
        "--build-root",
        type=Path,
        required=True,
        help="Parent of <env>/firmware.bin (e.g. Software/src/.pio/build)",
    )
    ap.add_argument(
        "--dropbox-root",
        type=Path,
        required=True,
        help="Dropbox firmware folder root (for collision checks).",
    )
    ap.add_argument(
        "--staging-dir",
        type=Path,
        required=True,
        help="Directory to place the renamed binaries into.",
    )
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args(argv)

    if args.channel == "beta":
        if not args.date or not re.fullmatch(r"\d{6}", args.date):
            print("--date YYMMDD is required for beta channel", file=sys.stderr)
            return 2
        if args.counter is None:
            print("--counter is required for beta channel", file=sys.stderr)
            return 2

    if not args.dropbox_root.is_dir():
        print(
            f"Dropbox root does not exist or is not a directory: {args.dropbox_root}",
            file=sys.stderr,
        )
        return 2

    def log(msg: str) -> None:
        prefix = "[dry-run] " if args.dry_run else ""
        print(prefix + msg, file=sys.stderr)

    result: dict = {}
    for key, (env, prefix, _subdir) in PLATFORMS.items():
        src = args.build_root / env / "firmware.bin"

        try:
            filename, manifest_version = compute_target(
                platform_key=key,
                channel=args.channel,
                version=args.version,
                date=args.date,
                counter=args.counter,
                dropbox_root=args.dropbox_root,
            )
        except RuntimeError as e:
            print(f"[{key}] {e}", file=sys.stderr)
            return 1

        dst = args.staging_dir / filename
        try:
            stage_binary(src, dst, args.dry_run, log)
        except FileNotFoundError as e:
            print(f"[{key}] {e}", file=sys.stderr)
            return 1

        _, _, subdir = PLATFORMS[key]
        result[key] = {
            "source": str(src),
            "filename": filename,
            "staged_path": str(dst),
            "manifest_version": manifest_version,
            "dropbox_subdir": subdir,  # "" for root, "m32p" for Pocket
        }

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
