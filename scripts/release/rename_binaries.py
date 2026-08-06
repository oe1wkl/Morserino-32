#!/usr/bin/env python3
"""
rename_binaries.py — stage renamed firmware binaries for a release.

For each release platform (M32 original, M32 Pocket, M32 Pocket Accessibility
Edition — see PLATFORMS):
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

Platforms that ship a filesystem image (the Accessibility Edition's voice
clips) also stage <env>/spiffs.bin, named after the application binary so the
same-day beta suffix carries over. Platforms with a build-specific partition
table also stage their bootloader/partition binaries into common/.

Output (stdout, one JSON object):
  {
    "m32":  {"source": "...", "filename": "fw_m32_V8.1.bin",
             "staged_path": "...", "manifest_version": "8.1",
             "dropbox_subdir": ""},
    "m32p": {"source": "...", "filename": "fw_m32p_V8.1.bin",
             "staged_path": "...", "manifest_version": "8.1",
             "dropbox_subdir": "m32p"},
    "m32pa11y": {"source": "...", "filename": "fw_m32pa11y_V9.0.bin",
             "staged_path": "...", "manifest_version": "9.0",
             "dropbox_subdir": "m32p-a11y",
             "fs_filename": "fs_m32pa11y_V9.0.bin",
             "fs_source": "...", "fs_staged_path": "...",
             "common_files": [{"name": "bootloader.bin", "staged_path": "..."},
                              {"name": "partitions.bin", "staged_path": "..."}]}
  }
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


def find_boot_app0() -> Path:
    """Locate boot_app0.bin, the OTA-selector image flashed at 0xE000.

    It is not a build output — it ships with the Arduino framework package, so
    `pio run` never copies it into .pio/build. (idedata.json does name it, but a
    plain `pio run` neither refreshes that file nor writes it for every
    environment, so it cannot be trusted here.) The content is identical for
    every ESP32 target; what matters is that each published target directory
    holds its own copy, so the target is self-contained.
    """
    roots = []
    if os.environ.get("PLATFORMIO_CORE_DIR"):
        roots.append(Path(os.environ["PLATFORMIO_CORE_DIR"]))
    roots.append(Path.home() / ".platformio")

    for root in roots:
        for candidate in sorted(
            root.glob("packages/framework-arduinoespressif32*/tools/partitions/boot_app0.bin")
        ):
            return candidate
    raise FileNotFoundError(
        "boot_app0.bin not found under "
        + " or ".join(str(r) for r in roots)
        + " — is the Arduino ESP32 framework installed for PlatformIO?"
    )

# Platform definitions.
#
#   env            PlatformIO environment under --build-root
#   prefix         filename prefix for the application binary
#   subdir         directory under the Dropbox firmware root ("" = root)
#   fs_prefix      filename prefix for the filesystem image (optional; only
#                  platforms that ship one, i.e. the Accessibility Edition)
#   common         publish the target's own common/ directory (optional).
#                  Only for platforms whose partition table is build-specific —
#                  the mainline two use hand-placed, long-stable common files.
#                  bootloader.bin and partitions.bin come from the build;
#                  boot_app0.bin comes from the framework (see find_boot_app0).
#   max_app_bytes  hard ceiling for firmware.bin (optional). The Accessibility
#                  Edition's app partition is 0.5 MB tighter than the mainline
#                  Pocket one, and nothing else would catch an overflow.
PLATFORMS = {
    "m32": {
        "env": "heltec_wifi_lora_32_V2",
        "prefix": "fw_m32",
        "subdir": "",
    },
    "m32p": {
        "env": "pocketwroom",
        "prefix": "fw_m32p",
        "subdir": "m32p",
    },
    "m32pa11y": {
        "env": "pocketwroom-accessibility",
        "prefix": "fw_m32pa11y",
        "subdir": "m32p-a11y",
        "fs_prefix": "fs_m32pa11y",
        "common": True,
        "max_app_bytes": 0x2D0000,   # app0 in m32pocket_accessibility.csv
    },
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
    spec = PLATFORMS[platform_key]
    prefix, subdir = spec["prefix"], spec["subdir"]
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
    for key, spec in PLATFORMS.items():
        build_dir = args.build_root / spec["env"]
        src = build_dir / "firmware.bin"

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

        # Overflowing a tight app partition produces a binary that flashes and
        # then does not boot, so fail the release instead.
        ceiling = spec.get("max_app_bytes")
        if ceiling and src.is_file() and src.stat().st_size > ceiling:
            print(
                f"[{key}] {src} is {src.stat().st_size:,} bytes but the app "
                f"partition holds only {ceiling:,}. Shrink the build or enlarge "
                f"the partition table.",
                file=sys.stderr,
            )
            return 1

        dst = args.staging_dir / filename
        try:
            stage_binary(src, dst, args.dry_run, log)
        except FileNotFoundError as e:
            print(f"[{key}] {e}", file=sys.stderr)
            return 1

        entry = {
            "source": str(src),
            "filename": filename,
            "staged_path": str(dst),
            "manifest_version": manifest_version,
            "dropbox_subdir": spec["subdir"],  # "" for root, "m32p" for Pocket
        }

        # Filesystem image, for platforms that ship one. Its name follows the
        # application's, so the same-day beta suffix carries over for free.
        if "fs_prefix" in spec:
            fs_src = build_dir / "spiffs.bin"
            if not fs_src.is_file():
                print(
                    f"[{key}] {fs_src} not found — the release workflow must run "
                    f"`pio run -e {spec['env']} -t buildfs` as well as the plain build.",
                    file=sys.stderr,
                )
                return 1
            fs_filename = spec["fs_prefix"] + filename[len(spec["prefix"]):]
            fs_dst = args.staging_dir / fs_filename
            try:
                stage_binary(fs_src, fs_dst, args.dry_run, log)
            except FileNotFoundError as e:
                print(f"[{key}] {e}", file=sys.stderr)
                return 1
            entry["fs_filename"] = fs_filename
            entry["fs_source"] = str(fs_src)
            entry["fs_staged_path"] = str(fs_dst)

        # Build-specific common files (bootloader / partition table). These are
        # shared by every version in that directory, so a change to them silently
        # re-pairs the older versions listed in the same manifest. Warn — a
        # deliberate partition change needs a human decision about the old entries.
        if spec.get("common"):
            dropbox_dir = (
                args.dropbox_root / spec["subdir"] if spec["subdir"] else args.dropbox_root
            )
            try:
                sources = [
                    build_dir / "bootloader.bin",
                    build_dir / "partitions.bin",
                    find_boot_app0(),
                ]
            except FileNotFoundError as e:
                print(f"[{key}] {e}", file=sys.stderr)
                return 1

            commons = []
            for c_src in sources:
                name = c_src.name
                c_dst = args.staging_dir / "common" / name
                try:
                    stage_binary(c_src, c_dst, args.dry_run, log)
                except FileNotFoundError as e:
                    print(f"[{key}] {e}", file=sys.stderr)
                    return 1
                published = dropbox_dir / "common" / name
                if published.is_file() and published.read_bytes() != c_src.read_bytes():
                    log(
                        f"WARNING: {name} differs from the published "
                        f"{published}. Older versions in this manifest will be "
                        f"paired with the new file — check they still boot."
                    )
                commons.append({"name": name, "staged_path": str(c_dst)})
            entry["common_files"] = commons

        result[key] = entry

    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
