#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
firmware_terms.py -- the on-device vocabulary, taken from the firmware itself.

The manual is full of strings that are really *device labels*: menu entries,
preference names, option values. Session 2 replaces the variant-dependent ones
with placeholders resolved from a per-variant terminology file, so session 1
has to say which ones those are -- and the only trustworthy source for that is
the firmware's own tables, not the prose.

Parsed, with #ifdef context kept:
  MorseMenu.cpp        menuText[]     -- menu entries
  MorsePreferences.cpp pliste[]       -- preference labels + their option values
  MorsePreferences.cpp extraItems[]   -- action items at the end of the list

Each term is reported with the build flags that gate it, which map onto the
variants like this:

  CONFIG_TFT, CONFIG_SOUND_I2S, CONFIG_PRACTICE_STATS, CONFIG_CN3_PADDLE
                                 -> Pocket only  (pocket + pocket-a11y)
  CONFIG_CW_GAME                 -> Pocket only, and NOT in the a11y edition
  !CONFIG_AUDIO_A11Y             -> everywhere except the a11y edition
  LORA_DISABLED / !LORA_DISABLED -> Pocket / classic respectively
  everything else                -> all variants
"""
import os
import re

SRC = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "Software", "src", "Version 6 and newer"))

POCKET_ONLY = {"CONFIG_TFT", "CONFIG_SOUND_I2S", "CONFIG_PRACTICE_STATS",
               "CONFIG_CN3_PADDLE", "CONFIG_DECODER_I2S", "LORA_DISABLED"}
CLASSIC_ONLY = {"!LORA_DISABLED"}
GAMES = {"CONFIG_CW_GAME"}
NOT_A11Y = {"!CONFIG_AUDIO_A11Y"}


def variants_for(guards):
    """Which of the three variant keys a guarded term belongs to."""
    v = {"classic", "pocket", "pocket-a11y"}
    for g in guards:
        if g in POCKET_ONLY:
            v &= {"pocket", "pocket-a11y"}
        elif g in CLASSIC_ONLY:
            v &= {"classic"}
        elif g in GAMES:
            v &= {"pocket"}
        elif g in NOT_A11Y:
            v -= {"pocket-a11y"}
    return sorted(v)


def _scan(text, collect):
    """Walk C source keeping a stack of #ifdef guards; call collect(line, guards)."""
    stack = []
    for line in text.split("\n"):
        s = line.strip()
        m = re.match(r"#\s*(ifdef|ifndef|if|else|elif|endif)\b\s*(.*)", s)
        if m:
            kw, rest = m.group(1), m.group(2).strip()
            rest = re.split(r"//|/\*", rest)[0].strip()   # drop trailing comment
            if kw == "ifdef":
                stack.append(rest)
            elif kw == "ifndef":
                stack.append("!" + rest)
            elif kw == "if":
                stack.append(rest)
            elif kw == "else" and stack:
                top = stack[-1]
                stack[-1] = top[1:] if top.startswith("!") else "!" + top
            elif kw == "endif" and stack:
                stack.pop()
            continue
        collect(line, tuple(stack))


def _slice(path, start_marker, end_marker):
    text = open(os.path.join(SRC, path), encoding="utf-8").read()
    a = text.index(start_marker)
    b = text.index(end_marker, a)
    return text[a:b]


def menu_entries():
    out = []
    body = _slice("MorseMenu.cpp", "const char* const menuText[menuN]",
                  "enum navi")

    def collect(line, guards):
        for q in re.findall(r'"([^"]*)"', line):
            if q:
                out.append((q, guards))
    _scan(body, collect)
    return out


def preferences():
    """(parName, option values, guards) for every pliste[] entry.

    A pliste[] record is  { defaults..., "parName", "help", bool, {options},
    "spokenName"? } -- so the *first* standalone string in a record is the
    display label and the second is the help text. Records are found by brace
    depth rather than by counting strings, which is what an earlier
    every-other-string heuristic got wrong (it reported help texts as labels).
    """
    body = _slice("MorsePreferences.cpp", "parameter MorsePreferences::pliste[]",
                  "const char* const extraItems[]")
    body = body.split("\n", 1)[1]        # drop the "... pliste[] = {" opener
    entries = []
    state = {"depth": 0, "rec": None}

    def collect(line, guards):
        s = line.strip()
        opens = s.count("{")
        closes = s.count("}")
        # a record starts at depth 0->1 on a line that is just "{"
        if state["depth"] == 0 and s.startswith("{"):
            state["rec"] = {"labels": [], "opts": [], "guards": guards}
        if state["rec"] is not None:
            standalone = re.match(
                r'^"((?:[^"\\]|\\.)*)",?\s*(?://.*)?$', s)
            if standalone:
                state["rec"]["labels"].append(standalone.group(1))
            opts = re.match(r"^\{\s*(\".*\")\s*\}\s*,?\s*$", s)
            if opts:
                state["rec"]["opts"] = re.findall(r'"([^"]*)"', opts.group(1))
        state["depth"] += opens - closes
        if state["depth"] <= 0 and state["rec"] is not None:
            rec = state["rec"]
            state["rec"] = None
            state["depth"] = 0
            if rec["labels"]:
                entries.append((rec["labels"][0], rec["opts"], rec["guards"]))
    _scan(body, collect)
    return entries


def extra_items():
    out = []
    body = _slice("MorsePreferences.cpp", "const char* const extraItems[]",
                  "\n}")

    def collect(line, guards):
        for q in re.findall(r'"([^"]+)"', line):
            out.append((q, guards))
    _scan(body, collect)
    return out


if __name__ == "__main__":
    print("== menu entries ==")
    for term, g in menu_entries():
        print("  %-20s %-28s %s" % (term, ",".join(g) or "-",
                                    "/".join(variants_for(g))))
    print("\n== preferences ==")
    for label, opts, g in preferences():
        print("  %-20s %-24s %s" % (label, ",".join(g) or "-",
                                    "/".join(variants_for(g))))
        if opts:
            print("      values: %s" % ", ".join(opts))
    print("\n== extra items ==")
    for term, g in extra_items():
        print("  %-20s %-24s %s" % (term, ",".join(g) or "-",
                                    "/".join(variants_for(g))))
