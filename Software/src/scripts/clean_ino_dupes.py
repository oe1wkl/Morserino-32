"""
PlatformIO pre-build hook: delete stale `<sketch>.ino N.cpp` duplicates.

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


src_dir = env.subst("$PROJECT_SRC_DIR")  # noqa: F821
_clean(src_dir)
