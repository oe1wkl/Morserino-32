"""
PlatformIO pre-build hook: delete stale `<sketch>.ino N.cpp` duplicates, and
iCloud/Finder "name N.ext" copies (second pass, at the end of this file).

Background
----------
PlatformIO converts `.ino` sketches into `.cpp` translation units as part
of the build. If a build is interrupted (Ctrl-C, IDE crash, USB
disconnect at the wrong moment) or if Finder/iCloud creates duplicates
of the .ino source, the source tree can be left with files like:

    Version 6 and newer/m32_v6.ino 2.cpp
    Version 6 and newer/m32_v6.ino 3.cpp

PIO then compiles both the canonical `m32_v6.ino.cpp` and the duplicates,
producing "multiple definition of …" linker errors for every symbol in
the sketch. The cure has always been a manual `rm` — this script makes
the cleanup automatic so a hand-fix is never needed again.

Pattern matched: any file whose name is `<anything>.ino <digits>.cpp` or
`<anything>.ino <digits>` (a Finder duplicate that has yet to be
re-extensioned). The space between `.ino` and the digit is what
distinguishes a Finder/PIO duplicate from a legitimate `.cpp` file.

Scope: limited to the project's `src_dir` (the directory PIO compiles
from). Other parts of the tree are untouched.
"""

import os
import re
from pathlib import Path

# Injected by PlatformIO at script load time.
Import("env")  # noqa: F821 - PIO injects this builtin

# Filenames like:   m32_v6.ino 2.cpp     m32_v6.ino 10.cpp     foo.ino 3
# But NOT:          m32_v6.ino.cpp       m32_v6_2.cpp          m32_v6 2.cpp
_DUPE_RE = re.compile(r".+\.ino \d+(\.cpp)?$")


def _clean(src_dir: str) -> int:
    """Remove any duplicate-pattern files under ``src_dir``. Returns count."""
    removed = 0
    for dirpath, _dirnames, filenames in os.walk(src_dir):
        for name in filenames:
            if _DUPE_RE.match(name):
                target = Path(dirpath) / name
                try:
                    target.unlink()
                    print(f"[clean_ino_dupes] removed stale duplicate: {target}")
                    removed += 1
                except OSError as exc:  # pragma: no cover - defensive
                    print(f"[clean_ino_dupes] could not remove {target}: {exc}")
    return removed


# Second pass: iCloud Drive / Finder conflict copies of ANY file - "MorseKipRig 2.cpp", "foo 3.h",
# "a1b2c3d4 2.mp3" (2026-10-03: the repo lives in iCloud-synced ~/Documents, and a branch switch that
# deletes and recreates a folder makes iCloud keep "name 2" copies - 82 in one afternoon). In src_dir a
# copy is compiled next to its original ("multiple definition of ..."); in data_dir it would be packed
# into the SPIFFS image, where the a11y voice store has little room to spare.
# A copy byte-identical to the original beside it is redundant and removed. Any other copy in src_dir -
# original missing, or different content - stops the build: which version is wanted is not ours to guess.
_COPY_RE = re.compile(r"^(?P<stem>.+) (?P<n>\d+)(?P<ext>\.[A-Za-z0-9]+)?$")
_SOURCE_EXT = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".ino", ".S", ".s"}


def _same(a: Path, b: Path) -> bool:
    try:
        return a.stat().st_size == b.stat().st_size and a.read_bytes() == b.read_bytes()
    except OSError:
        return False


def _clean_copies(root: str, sources_only: bool) -> list:
    """Remove identical "name N.ext" copies under root; return the ones that could not be judged safe."""
    unsafe = []
    if not os.path.isdir(root):
        return unsafe
    for dirpath, _dirnames, filenames in os.walk(root):
        for name in filenames:
            m = _COPY_RE.match(name)
            if not m:
                continue
            ext = m.group("ext") or ""
            if sources_only and ext not in _SOURCE_EXT:
                continue
            copy = Path(dirpath) / name
            original = Path(dirpath) / (m.group("stem") + ext)
            if original.exists() and _same(copy, original):
                try:
                    copy.unlink()
                    print(f"[clean_ino_dupes] removed identical iCloud/Finder copy: {copy}")
                except OSError as exc:  # pragma: no cover - defensive
                    print(f"[clean_ino_dupes] could not remove {copy}: {exc}")
                    unsafe.append(copy)
            elif sources_only:
                unsafe.append(copy)
    return unsafe


src_dir = env.subst("$PROJECT_SRC_DIR")  # noqa: F821
_clean(src_dir)
_unsafe = _clean_copies(src_dir, sources_only=True)
_clean_copies(env.subst("$PROJECT_DATA_DIR"), sources_only=False)  # noqa: F821 - SPIFFS image source
if _unsafe:
    print("\n[clean_ino_dupes] STOP: these look like iCloud/Finder copies of source files, but they are not")
    print("identical to an original beside them, so they would be compiled as well. Delete or rename them:")
    for f in _unsafe:
        print(f"    {f}")
    env.Exit(1)  # noqa: F821
