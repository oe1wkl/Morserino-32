#!/usr/bin/env python3
"""
check_dropbox_targets.py -- verify the Dropbox publish targets before building.

Why this exists
---------------
The first V9.0-beta.1 run spent its whole 30-minute budget inside the staging
step, produced not one line of output, and was killed by the job timeout. Two
independent causes, either of which is enough to lose a release:

  * m32p-a11y/ did not exist in the Dropbox firmware root. dropbox_publish.sh
    deliberately refuses to create a platform directory (it only creates
    common/), so the run was going to fail at the publish step regardless --
    after four builds had already run.
  * MORSERINO_DROPBOX_ROOT lives under ~/Library/CloudStorage, which is a macOS
    file provider. While the provider is asleep, signed out or offline, a plain
    stat() on that path blocks, with no output and no error, until something
    else gives up.

Both are cheap to detect up front, so this runs before the builds: a missing
directory becomes a one-line error instead of a 30-minute timeout, and a
stalled provider is reported as a stalled provider rather than looking like a
hung build.

Each filesystem probe runs in a forked child with a deadline. That matters: a
signal.alarm() inside this process cannot promise to interrupt a stat() that is
stuck in the kernel, whereas the parent here never touches the path itself and
can always report. A child that will not die is left to the runner's orphan
cleanup -- by then this process has already exited with a useful message.

Usage
-----
  check_dropbox_targets.py                    # uses $MORSERINO_DROPBOX_ROOT
  check_dropbox_targets.py --root /some/path  # explicit
  check_dropbox_targets.py --timeout 30       # per-probe deadline, seconds

Exit codes: 0 all targets usable, 2 a target is missing or not writable,
3 a probe timed out (provider stalled), 4 the root is unset or unusable.
"""

from __future__ import annotations

import argparse
import os
import signal
import sys
import time
import warnings

# Python 3.12 warns on fork() from a process that has threads, because a child
# can inherit a lock held by a thread that does not exist in it. That hazard
# does not apply here: the child runs three os.path calls and os._exit, taking
# no lock and importing nothing. The alternative -- spawning an interpreter per
# probe -- costs more and buys nothing, so silence just this warning rather
# than let it appear in every release log.
warnings.filterwarnings("ignore", category=DeprecationWarning,
                        message=r".*fork\(\) may lead to deadlocks.*")

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rename_binaries import PLATFORMS  # noqa: E402  (single source of truth)

# What a probe found. Kept as plain ints because they cross a fork boundary as
# process exit codes.
OK, MISSING, NOT_A_DIR, NOT_WRITABLE = 0, 1, 2, 3


def probe(path: str, timeout: float) -> tuple[str, float]:
    """Stat `path` in a child process. Returns (verdict, elapsed_seconds).

    verdict is one of "ok", "missing", "not-a-dir", "not-writable", "timeout".
    """
    started = time.monotonic()
    pid = os.fork()
    if pid == 0:                                    # child: the only code that
        try:                                        # touches the provider
            if not os.path.exists(path):
                os._exit(MISSING)
            if not os.path.isdir(path):
                os._exit(NOT_A_DIR)
            if not os.access(path, os.W_OK | os.X_OK):
                os._exit(NOT_WRITABLE)
            os._exit(OK)
        except Exception:
            os._exit(NOT_A_DIR)

    deadline = started + timeout
    while time.monotonic() < deadline:
        done, status = os.waitpid(pid, os.WNOHANG)
        if done:
            elapsed = time.monotonic() - started
            code = os.waitstatus_to_exitcode(status)
            return ({OK: "ok", MISSING: "missing", NOT_A_DIR: "not-a-dir",
                     NOT_WRITABLE: "not-writable"}.get(code, "not-a-dir"),
                    elapsed)
        time.sleep(0.05)

    # Past the deadline. Ask the child to die, but do not wait on it -- if it is
    # wedged in the kernel, waiting is exactly the mistake this check exists to
    # avoid.
    try:
        os.kill(pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    return "timeout", time.monotonic() - started


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--root", default=os.environ.get("MORSERINO_DROPBOX_ROOT"),
                    help="Dropbox firmware root (default: $MORSERINO_DROPBOX_ROOT)")
    ap.add_argument("--timeout", type=float, default=20.0,
                    help="per-probe deadline in seconds (default: 20)")
    args = ap.parse_args(argv)

    if not args.root:
        print("MORSERINO_DROPBOX_ROOT is not set, and no --root was given.",
              file=sys.stderr)
        return 4

    # The root first: if the provider is stalled, every subdirectory below it
    # would time out too, and one clear message beats three.
    verdict, elapsed = probe(args.root, args.timeout)
    if verdict == "timeout":
        print(f"Dropbox root did not respond within {args.timeout:g}s: {args.root}\n"
              f"  This path is a macOS file provider. Check that Dropbox is "
              f"running and signed in, open the folder in Finder once, then "
              f"re-run the release.", file=sys.stderr)
        return 3
    if verdict != "ok":
        print(f"Dropbox root is {verdict}: {args.root}", file=sys.stderr)
        return 4
    # flush=True throughout: stdout is block-buffered when the runner captures
    # it, so without this the progress lines land in the log *after* the error
    # they were supposed to give context for.
    print(f"root ok ({elapsed:.2f}s): {args.root}", flush=True)

    # Then one directory per platform, taken from the same table the staging
    # and publish steps use, so a new platform cannot be forgotten here —
    # plus the site's manuals/ directory, which lives beside firmware/ rather
    # than inside it and is where the browser tools' manual links point.
    targets = [(key, spec["subdir"], args.root) for key, spec in PLATFORMS.items()]
    targets.append(("manuals", "manuals", os.path.dirname(args.root.rstrip(os.sep))))

    failures = 0
    stalled = False
    for key, subdir, base in targets:
        path = os.path.join(base, subdir) if subdir else base
        verdict, elapsed = probe(path, args.timeout)
        label = f"{key} ({subdir or '<root>'})"

        if verdict == "ok":
            print(f"  ok ({elapsed:.2f}s): {label}", flush=True)
            continue

        failures += 1
        sys.stdout.flush()
        if verdict == "timeout":
            stalled = True
            print(f"  TIMEOUT after {elapsed:.0f}s: {label} -> {path}\n"
                  f"    The file provider is not answering for this path.",
                  file=sys.stderr)
        elif verdict == "missing":
            print(f"  MISSING: {label} -> {path}\n"
                  f"    dropbox_publish.sh will not create a platform "
                  f"directory; create it by hand (mkdir -p) and re-run. This is "
                  f"what a newly added platform needs before its first release.",
                  file=sys.stderr)
        else:
            print(f"  {verdict.upper()}: {label} -> {path}", file=sys.stderr)

    if failures:
        sys.stdout.flush()
        print(f"\n{failures} Dropbox target(s) unusable — stopping before the "
              f"builds rather than after them.", file=sys.stderr)
        # 3 if anything stalled, not merely if the *last* one did: a stall is
        # the more actionable diagnosis and must not be masked by a later
        # platform that happened to answer.
        return 3 if stalled else 2

    print("All Dropbox publish targets exist and are writable.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
