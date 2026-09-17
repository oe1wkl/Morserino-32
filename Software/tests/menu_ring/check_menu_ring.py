#!/usr/bin/env python3
"""
check_menu_ring.py -- every menu level must be a closed ring, in every build.

The mode menu is the table menuNav[] in MorseMenu.cpp. Each entry names its
own left and right neighbour, its parent (up) and its first child (down), so
every link is recorded twice -- once at each end -- and nothing but care keeps
the two ends in step. A link recorded at one end only compiles fine and shows
up as a menu that behaves differently turning one way than the other: in V9.0
stepping back from Koch Trainer's CW Generator skipped Preview Char, because
the entry had been moved and one of its two back-links was not (PR #222).

The table is also full of #ifdefs (LoRa, games, QSO Bot, Practice Stats, the
Accessibility Edition), so a ring that is closed in one build can be broken in
another. This script therefore checks every environment in platformio.ini:

  1. it asks PlatformIO for the environment's resolved build_flags (minus
     build_unflags) and keeps the -D macros;
  2. runs morsedefs.h and MorseMenu.cpp, with their #include lines removed,
     through the host C preprocessor with those macros -- so it sees exactly
     the enum, table and #defines that environment compiles;
  3. checks that
       - the enum, menuN, menuNav[] and menuText[] agree on the entry count
         (a short initializer compiles silently, zero-filled);
       - every left/right link is mutual, and joins entries of the same level
         under the same parent;
       - every down link names an entry one level deeper that points back up;
       - every entry can be reached from the top-level ring.

Usage (from anywhere in the repository):
  check_menu_ring.py                  # all environments; exit 1 on any fault
  check_menu_ring.py pocketwroom ...  # only these environments

Needs PlatformIO (`pio`) on PATH and a C/C++ preprocessor (`cc`, or $CC).
The idea and the first version of this check are Christian Konecny's, OE1CKO.
"""
import json
import os
import re
import shlex
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
PIO_PROJECT = os.path.join(REPO, "Software", "src")
FIRMWARE = os.path.join(PIO_PROJECT, "Version 6 and newer")
SOURCES = ("morsedefs.h", "MorseMenu.cpp")

LEVEL, LEFT, RIGHT, UP, DOWN = range(5)


# ------------------------------------------------------------ environments
def pio_environments():
    """{env name: [(macro, value or None)]} from PlatformIO's resolved config."""
    try:
        out = subprocess.run(["pio", "project", "config", "-d", PIO_PROJECT,
                              "--json-output"], capture_output=True, text=True,
                             check=True).stdout
    except FileNotFoundError:
        sys.exit("PlatformIO ('pio') is not on PATH -- it is needed to resolve "
                 "each environment's build flags.")
    except subprocess.CalledProcessError as e:
        sys.exit("pio project config failed:\n%s" % e.stderr)
    envs = {}
    for section, options in json.loads(out):
        if not section.startswith("env:"):
            continue
        opts = dict(options)
        flags = defines(opts.get("build_flags", []))
        unflags = defines(opts.get("build_unflags", []))
        envs[section[4:]] = [d for d in flags
                             if not any(d[0] == n and (v is None or d[1] == v)
                                        for n, v in unflags)]
    return envs


def defines(items):
    """[(macro, value or None)] for the -D flags among PlatformIO flag lines."""
    if isinstance(items, str):
        items = [items]
    out = []
    for item in items:
        tokens = shlex.split(item)
        for i, tok in enumerate(tokens):
            if tok == "-D" and i + 1 < len(tokens):
                body = tokens[i + 1]
            elif tok.startswith("-D") and len(tok) > 2:
                body = tok[2:]
            else:
                continue
            name, _, value = body.partition("=")
            out.append((name, value if _ else None))
    return out


# ------------------------------------------------------------ preprocessing
def rosetta():
    """True when this Python runs translated (x86_64 under Rosetta on Apple Silicon)."""
    if sys.platform != "darwin":
        return False
    res = subprocess.run(["sysctl", "-n", "sysctl.proc_translated"],
                         capture_output=True, text=True)
    return res.stdout.strip() == "1"


def preprocess(macros):
    unit = []
    for name in SOURCES:
        with open(os.path.join(FIRMWARE, name), encoding="utf-8") as fh:
            unit.extend(line for line in fh
                        if not re.match(r"\s*#\s*include\b", line))
        unit.append("\n")
    cmd = [os.environ.get("CC", "cc"), "-E", "-P", "-x", "c++", "-"]
    cmd += ["-D%s=%s" % (n, v) if v is not None else "-D%s" % n
            for n, v in macros]
    if rosetta():
        # An x86_64 Python (e.g. miniforge) on Apple Silicon passes its architecture
        # on to child processes, and Apple's cc shim then fails to load in xcrun.
        cmd = ["arch", "-arm64"] + cmd
    res = subprocess.run(cmd, input="".join(unit), capture_output=True,
                         text=True)
    if res.returncode != 0:
        raise RuntimeError("preprocessor failed:\n" + res.stderr)
    return res.stdout


def initializer(text, pattern, what):
    """Top-level comma-separated items of the brace initializer after pattern."""
    m = re.search(pattern, text)
    if not m:
        raise RuntimeError("could not find %s in the preprocessed sources" % what)
    i = text.index("{", m.end() - 1)
    items, depth, start, quote = [], 0, i + 1, None
    while i < len(text):
        c = text[i]
        if quote:
            if c == "\\":
                i += 1
            elif c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                items.append(text[start:i])
                return [s.strip() for s in items if s.strip()]
        elif c == "," and depth == 1:
            items.append(text[start:i])
            start = i + 1
        i += 1
    raise RuntimeError("unterminated initializer for %s" % what)


def menu_model(text):
    m = re.search(r"enum\s+menuNo\s*\{([^}]*)\}", text)
    if not m:
        raise RuntimeError("could not find enum menuNo")
    names, value = {}, 0
    for item in (s.strip() for s in m.group(1).split(",")):
        if not item:
            continue
        name, _, explicit = (p.strip() for p in item.partition("="))
        if explicit:
            value = int(explicit, 0)
        names[name] = value
        value += 1
    by_value = {v: n for n, v in names.items()}

    m = re.search(r"const\s+uint8_t\s+menuN\s*=\s*([^;]*);", text)
    if not m or not re.fullmatch(r"[\d\s+\-]+", m.group(1)):
        raise RuntimeError("could not evaluate menuN")
    menu_n = sum(int(t) for t in re.findall(r"[+\-]?\d+",
                                            re.sub(r"\s+", "", m.group(1))))

    rows = []
    for row in initializer(text, r"menuNav\s*\[\s*menuN\s*\]\s*\[\s*5\s*\]\s*=\s*\{",
                           "menuNav[]"):
        fields = [f.strip() for f in row.strip("{} \n\t").split(",")]
        if len(fields) != 5:
            raise RuntimeError("menuNav row without 5 fields: {%s}" % row)
        rows.append([int(f, 0) if re.fullmatch(r"\d+", f) else names[f]
                     if f in names else _unknown(f) for f in fields])

    labels = [" ".join(re.findall(r'"((?:\\.|[^"\\])*)"', item))
              for item in initializer(text, r"menuText\s*\[\s*menuN\s*\]\s*=\s*\{",
                                      "menuText[]")]
    return names, by_value, menu_n, rows, labels


def _unknown(symbol):
    raise RuntimeError("menuNav[] uses %r, which is not a menuNo enumerator" % symbol)


# ------------------------------------------------------------------ checks
def check(names, by_value, menu_n, rows, labels):
    faults = []
    counts = {"enum menuNo": len(names), "menuN": menu_n,
              "menuNav[] rows": len(rows), "menuText[] entries": len(labels)}
    if len(set(counts.values())) != 1:
        faults.append("entry counts disagree: " +
                      ", ".join("%s %d" % kv for kv in counts.items()))
        return faults            # every later index would be shifted: stop here

    def who(i):
        label = labels[i] if i < len(labels) else "?"
        return "%s [%s]" % (label.strip() or "-", by_value.get(i, i))

    n = len(rows)
    for i in range(1, n):
        level, left, right, up, down = rows[i]
        for side, other, back in (("left", left, RIGHT), ("right", right, LEFT)):
            if not 0 < other < n:
                faults.append("%s: %s neighbour is %d, not a menu entry"
                              % (who(i), side, other))
                continue
            if rows[other][back] != i:
                faults.append("%s: %s neighbour is %s, but that entry's %s "
                              "neighbour is %s -- one-way link"
                              % (who(i), side, who(other),
                                 "right" if back == RIGHT else "left",
                                 who(rows[other][back])))
            if rows[other][LEVEL] != level or rows[other][UP] != up:
                faults.append("%s: %s neighbour %s is not in the same ring "
                              "(level %d under %s, vs level %d under %s)"
                              % (who(i), side, who(other), rows[other][LEVEL],
                                 who(rows[other][UP]), level, who(up)))
        if level == 0 and up != 0:
            faults.append("%s: top-level entry with a parent (%s)" % (who(i), who(up)))
        if level > 0 and not (0 < up < n and rows[up][LEVEL] == level - 1):
            faults.append("%s: level %d entry whose parent is %s"
                          % (who(i), level, who(up) if 0 <= up < n else up))
        if down:
            if not 0 < down < n:
                faults.append("%s: down link %d is not a menu entry" % (who(i), down))
            elif rows[down][UP] != i or rows[down][LEVEL] != level + 1:
                faults.append("%s: down link goes to %s, which does not point "
                              "back up to it one level deeper" % (who(i), who(down)))

    # reachability: walk the top-level ring, then every ring hanging below it
    seen, todo = set(), [names.get("_keyer", 1)]
    while todo:
        start = entry = todo.pop()
        for _ in range(n):
            if entry in seen or not 0 < entry < n:
                break
            seen.add(entry)
            if rows[entry][DOWN]:
                todo.append(rows[entry][DOWN])
            entry = rows[entry][RIGHT]
            if entry == start:
                break
    for i in range(1, n):
        if i not in seen:
            faults.append("%s: cannot be reached from the menu" % who(i))
    return faults


# -------------------------------------------------------------------- main
def main(argv):
    envs = pio_environments()
    wanted = argv or sorted(envs)
    unknown = [e for e in wanted if e not in envs]
    if unknown:
        sys.exit("unknown environment(s): %s" % ", ".join(unknown))
    failed = 0
    for env in wanted:
        try:
            model = menu_model(preprocess(envs[env]))
            faults = check(*model)
        except (RuntimeError, KeyError, ValueError) as e:
            faults = ["could not check: %s" % e]
            model = None
        if faults:
            failed += 1
            print("FAIL: %s" % env)
            for f in faults:
                print("      %s" % f)
        else:
            print("OK:   %s (%d entries)" % (env, len(model[3]) - 1))
    if failed:
        print("\n%d of %d environment(s) have menu ring faults." % (failed, len(wanted)))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
