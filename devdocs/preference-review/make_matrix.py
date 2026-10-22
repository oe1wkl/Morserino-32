#!/usr/bin/env python3
"""
make_matrix.py -- working document for the V10 preference review (TODO B16/B17).

Builds PREFERENCE_MATRIX.md next to this script from the firmware's own tables, so it can be
regenerated after any reordering:

  * the prefPos enum, pliste[] (display names) and every per-mode option list in
    MorsePreferences.cpp, read from the C PREPROCESSOR output of each build environment
    (so every #ifdef and helper macro is resolved exactly as the compiler sees it);
  * the preference tables in the V10 user manual (EN), section by section;
  * the source lines that read each preference (pliste[posX]), as evidence of where it acts.

Run from anywhere:  python3 devdocs/preference-review/make_matrix.py
Needs PlatformIO (it runs `pio run -t compiledb` per environment; nothing is built).
"""
import json, os, re, shlex, subprocess, sys, tempfile, collections

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC_ROOT = os.path.join(REPO, "Software", "src")
SRC = os.path.join(SRC_ROOT, "Version 6 and newer")
MANUAL = os.path.join(REPO, "Documentation", "User Manual", "Version 10.x", "manual_en.md")
OUT = os.path.join(HERE, "PREFERENCE_MATRIX.md")

ENVS = [("pocketwroom", "Pocket"), ("pocketwroom-accessibility", "Pocket a11y"),
        ("heltec_wifi_lora_32_V2", "Classic")]

# option list -> the menu entries that use it (from the setCurrentOptions() calls in MorseMenu.cpp::menuExec)
LISTS = [
    ("keyerOptions",       "CW Keyer",             "Keyer"),
    ("generatorOptions",   "CW Generator (Random, Abbrevs, Words, Calls, Mixed, Practice Set)", "Gen"),
    ("playerOptions",      "CW Generator: File Player", "GenFile"),
    ("echoTrainerOptions", "Echo Trainer (Random ... Practice Set)", "Echo"),
    ("echoPlayerOptions",  "Echo Trainer: File Player", "EchoFile"),
    ("kochGenOptions",     "Koch Trainer: CW Generator", "KochGen"),
    ("kochEchoOptions",    "Koch Trainer: Select Lesson, Learn New Chr, Preview Char, Echo Trainer, Adaptive", "KochEcho"),
    ("loraTrxOptions",     "LoRa Trx", "LoRa"),
    ("wifiTrxOptions",     "WiFi Trx", "WiFi"),
    ("extTrxOptions",      "iCW/Ext Trx", "ExtTrx"),
    ("kipKeyerOptions",    "Remote Keyer", "RKeyer"),
    ("kipRigOptions",      "Remote Rig (tools only; no prefs gesture in the mode)", "RRig"),
    ("qsoBotOptions",      "QSO Bot", "Bot"),
    ("decoderOptions",     "CW Decoder", "Decoder"),
]


def preprocess(env, tmp):
    """C-preprocess MorsePreferences.cpp exactly as `env` compiles it; return the text."""
    subprocess.run(["pio", "run", "-e", env, "-t", "compiledb"], cwd=SRC_ROOT, check=True,
                   capture_output=True)
    ccpath = os.path.join(SRC_ROOT, "compile_commands.json")
    cc = json.load(open(ccpath))
    os.remove(ccpath)                                  # generated; never commit it
    ent = [c for c in cc if c["file"].endswith("MorsePreferences.cpp")][0]
    args = ent.get("arguments") or shlex.split(ent["command"])
    out, skip = [], False
    for a in args:
        if skip: skip = False; continue
        if a == "-o": skip = True; continue
        if a == "-c": continue
        out.append(a)
    ii = os.path.join(tmp, env + ".ii")
    subprocess.run(out + ["-E", "-P", "-o", ii], cwd=ent["directory"], check=True, capture_output=True)
    return open(ii, encoding="utf-8", errors="replace").read()


def braced(text, start):
    """Return the text between the '{' at/after `start` and its matching '}'."""
    i = text.index("{", start)
    depth, j, in_str = 0, i, False
    while True:
        c = text[j]
        if in_str:
            if c == "\\": j += 1
            elif c == '"': in_str = False
        elif c == '"': in_str = True
        elif c == "{": depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0: return text[i + 1:j]
        j += 1


def top_level_items(body):
    """Split a braced initializer body into its top-level {...} items."""
    items, depth, cur, in_str, k = [], 0, [], False, 0
    while k < len(body):
        c = body[k]
        if in_str:
            cur.append(c)
            if c == "\\": cur.append(body[k + 1]); k += 1
            elif c == '"': in_str = False
        elif c == '"': in_str = True; cur.append(c)
        elif c == "{":
            depth += 1
            if depth == 1: cur = []
            else: cur.append(c)
        elif c == "}":
            depth -= 1
            if depth == 0: items.append("".join(cur))
            else: cur.append(c)
        elif depth >= 1: cur.append(c)
        k += 1
    return items


def parse(pp):
    enum_body = braced(pp, re.search(r"enum\s+prefPos\s*:\s*uint8_t\s*\{", pp).start())
    enum = [t.strip() for t in enum_body.split(",") if t.strip()]
    enum = [t.split("=")[0].strip() for t in enum]
    pl = top_level_items(braced(pp, pp.index("MorsePreferences::pliste[] =")))
    names = [re.search(r'"((?:[^"\\]|\\.)*)"', it).group(1) for it in pl]
    # action items after posSerialOut have no pliste[] entry; their labels are extraItems[], from posKochFilter on
    m = re.search(r"extraItems\[\]\s*=", pp)
    if m and "posKochFilter" in enum:
        extra = re.findall(r'"((?:[^"\\]|\\.)*)"', braced(pp, m.start()))
        k0 = enum.index("posKochFilter")
        names = names + [None] * (k0 - len(names)) + extra
    lists = {}
    for lst, _, _ in LISTS + [("allOptions", "", "")]:
        m = re.search(r"MorsePreferences::%s\[\]\s*=" % lst, pp)
        if not m: continue
        lists[lst] = [t.strip() for t in braced(pp, m.start()).split(",") if t.strip()]
    return enum, names, lists


def manual_tables():
    """[(section, [names in table order])] from the 'List of All ... preferences' chapter."""
    md = open(MANUAL, encoding="utf-8").read()
    start = md.index("## List of All Morserino-32 preferences")
    end = md.index("\n## ", start + 10)
    secs = []
    for m in re.finditer(r"^### (.+)$", md[start:end], re.M):
        s0 = start + m.end()
        nxt = re.search(r"^### ", md[s0:end], re.M)
        chunk = md[s0: s0 + nxt.start()] if nxt else md[s0:end]
        rows = []
        for line in chunk.splitlines():
            if line.startswith("|") and not line.startswith("|---") and "Preference Name" not in line:
                cell = line.split("|")[1].strip()
                cell = re.sub(r"\{[^}]*\}", "", cell).replace("[", "").replace("]", "")
                cell = cell.replace("\\<", "<").replace("\\>", ">").replace("*", "").strip()
                if cell: rows.append(cell)
        secs.append((m.group(1).strip(), rows))
    return secs


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def read_sites():
    """posX -> Counter(file -> number of pliste[posX] reads)."""
    sites = collections.defaultdict(collections.Counter)
    for fn in sorted(os.listdir(SRC)):
        if not fn.endswith((".cpp", ".ino", ".h")) or fn.startswith("voice_clips"): continue
        for m in re.finditer(r"pliste\[\s*(pos\w+)\s*\]", open(os.path.join(SRC, fn), errors="replace").read()):
            sites[m.group(1)][fn] += 1
    return sites


def main():
    data = {}
    with tempfile.TemporaryDirectory() as tmp:
        for env, label in ENVS:
            data[label] = parse(preprocess(env, tmp))
    enumP, namesP, listsP = data["Pocket"]
    name_of = {lbl: {p: x for p, x in zip(e, n) if x} for lbl, (e, n, _) in data.items()}
    for lbl, (e, n, _) in data.items():
        # prefPos values after posSerialOut are action items, not in pliste[]: name them from the enum
        for p in e[len(n):]:
            name_of[lbl].setdefault(p, "(" + p[3:] + ")")
    def nm(p):
        for lbl in name_of:
            if p in name_of[lbl]: return name_of[lbl][p]
        return p

    L = []
    w = L.append
    w("# Preference review — working document (TODO B16 / B17)\n")
    w("Generated by `make_matrix.py` from the firmware's own tables (C-preprocessed per build) and the V10")
    w("manual (EN). **Do not edit by hand** — change the firmware or the manual and re-run. The decisions belong")
    w("in `REVIEW_NOTES.md` next to this file.\n")
    w("Columns are the M32 Pocket build unless a cell says otherwise. Numbers are the position at which the")
    w("preference appears in that mode's list (what the operator scrolls through on a double-click).\n")

    # ---- 1. the "All" view, per build, against the manual -------------------------------------
    secs = manual_tables()
    man_pos = {}
    for si, (sec, rows) in enumerate(secs):
        for ri, r in enumerate(rows):
            man_pos[norm(r)] = (si, ri, sec, r)
    w("## 1. The \"All\" view (preferences entered from the top menu) vs. the manual\n")
    w("Device order = `allOptions[]`. Manual = section and row of the preference tables in chapter")
    w("*List of All Morserino-32 preferences*. ⚠ = the manual's order disagrees with the device here")
    w("(manual position goes backwards relative to the previous row).\n")
    w("| # | Preference (device name) | prefPos | Classic | a11y | Manual section | row |")
    w("|---:|---|---|:-:|:-:|---|---:|")
    allP = listsP["allOptions"]
    allC = set(data["Classic"][2]["allOptions"]); allA = set(data["Pocket a11y"][2]["allOptions"])
    prev = (-1, -1); unmatched_dev = []
    for i, p in enumerate(allP, 1):
        n = nm(p)
        mp = man_pos.get(norm(n))
        if not mp:
            # a few manual names carry a unit or a fuller form; try prefix matches both ways
            cands = [v for k, v in man_pos.items() if k.startswith(norm(n)) or norm(n).startswith(k)]
            mp = cands[0] if len(cands) == 1 else None
        flag = ""
        if mp:
            if (mp[0], mp[1]) < prev: flag = " ⚠"
            prev = (mp[0], mp[1])
            sec, row = mp[2], str(mp[1] + 1)
        else:
            sec, row = "**not found in manual**", ""
            unmatched_dev.append(n)
        w(f"| {i} | {n}{flag} | `{p}` | {'✓' if p in allC else '—'} | {'✓' if p in allA else '—'} | {sec} | {row} |")
    extraC = [p for p in data["Classic"][2]["allOptions"] if p not in set(allP)]
    extraA = [p for p in data["Pocket a11y"][2]["allOptions"] if p not in set(allP)]
    if extraC or extraA:
        w("\nOnly in other builds: " + ", ".join(f"{nm(p)} (`{p}`, Classic)" for p in extraC) +
          (", " if extraC and extraA else "") + ", ".join(f"{nm(p)} (`{p}`, a11y)" for p in extraA))
    dev_norm = {norm(nm(p)) for lbl in data for p in data[lbl][2]["allOptions"]}
    man_only = [(sec, r) for sec, rows in secs for r in rows
                if norm(r) not in dev_norm and not any(d.startswith(norm(r)) or norm(r).startswith(d) for d in dev_norm)]
    if man_only:
        w("\nIn the manual but not matched to a device preference (name differs, or not in the All view):\n")
        for sec, r in man_only: w(f"- {r} — *{sec}*")
    w("")

    # ---- 2. mode x preference matrix ---------------------------------------------------------------
    w("## 2. Which preferences each mode offers\n")
    w("Column key:\n")
    for lst, desc, short in LISTS: w(f"- **{short}** — {desc} (`{lst}`)")
    w("\nA number = offered, at that position in the mode's list. `C` / `A` after a cell = differs on the Classic /")
    w("a11y build (`C—` not offered there, `C+` offered only there). Rows in All-view order; preferences no mode")
    w("offers are listed after the table.\n")
    shorts = [s for _, _, s in LISTS]
    w("| Preference | " + " | ".join(shorts) + " |")
    w("|---|" + "|".join(":-:" for _ in shorts) + "|")
    order = list(allP) + [p for lbl in data for p in data[lbl][2]["allOptions"] if p not in allP]
    seen = set(); never = []
    for p in order:
        if p in seen: continue
        seen.add(p)
        cells, anywhere = [], False
        for lst, _, _ in LISTS:
            c = ""
            inP = p in listsP.get(lst, [])
            if inP: c = str(listsP[lst].index(p) + 1); anywhere = True
            for lbl, tag in (("Classic", "C"), ("Pocket a11y", "A")):
                other = data[lbl][2].get(lst)
                if other is None: continue
                if inP and p not in other: c += f" {tag}—"
                if not inP and p in other: c += f" {tag}+"; anywhere = True
            cells.append(c)
        if anywhere: w(f"| {nm(p)} | " + " | ".join(cells) + " |")
        else: never.append(p)
    if never:
        w("\nOffered in no mode (All view only): " + ", ".join(f"{nm(p)}" for p in never) + "\n")

    # ---- 3. where each preference is read ----------------------------------------------------------
    sites = read_sites()
    w("## 3. Where each preference is read in the source\n")
    w("Count of `pliste[posX]` reads per file — evidence for B17 (*does it act in the modes that offer it?*).")
    w("A preference read only in `MorsePreferences.cpp` / `MorseJSON.cpp` is set and stored but acts elsewhere")
    w("through a cached variable; check by hand.\n")
    w("| Preference | prefPos | read in |")
    w("|---|---|---|")
    for p in order:
        s = sites.get(p)
        w(f"| {nm(p)} | `{p}` | " + (", ".join(f"{f} ×{k}" for f, k in s.most_common()) if s else "—") + " |")
    w("")
    open(OUT, "w", encoding="utf-8").write("\n".join(L) + "\n")
    print("wrote", OUT)


if __name__ == "__main__":
    main()
