#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
extract_voice_strings.py -- Morserino-32 V9.0 audio accessibility (M32 Pocket)

Emits every distinct clip the on-device voice engine needs, in two groups:

  * PHRASES  -- one clip each: menu entries, preference labels (spokenName, else
                parName), option values, action labels, unit words.
  * ATOMS    -- composed at playback: NATO phonetic letters, "pro sign", "error",
                punctuation names, integers (0..60 + multiples of 5 to 250).
                Prosigns / snapshot readouts / "NN char X" / the WpM-volume HUD are
                built by *sequencing* atoms, so no per-prosign clip is stored.

Reads the firmware's own tables (config-aware for the pocketwroom build) so the set
regenerates when entries change. Outputs (next to this script):
  voice_strings.txt   all distinct clip texts, deduped + sorted  (feeds generate_audio.sh)
  voice_manifest.json phrases{text:slug}, characters{char:[slug,...]}, collisions, counts

Decisions (maintainer, 2026-06-23): spoken label = dedicated field; letters = NATO
phonetic; prosigns = "pro sign" + phonetic letters composed from atoms.
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.environ.get("M32_SRC", os.path.normpath(
    os.path.join(HERE, "..", "..", "src", "Version 6 and newer")))

POCKET_MACROS = {
    "CONFIG_TFT", "CONFIG_CW_GAME", "CONFIG_QSO_BOT", "CONFIG_SOUND_I2S",
    "CONFIG_BLUETOOTH_KEYBOARD", "LORA_DISABLED", "CONFIG_ENGLISH_OXFORD",
    "CONFIG_TLV320AIC3100", "CONFIG_MCP73871", "CONFIG_DECODER_I2S",
    "CONFIG_BATMEAS_PIN", "ARDUINO_USB_MODE", "ARDUINO_USB_CDC_ON_BOOT",
}
QSTRING = re.compile(r'"((?:[^"\\]|\\.)*)"')

# ── NATO phonetic alphabet + how single characters are voiced ────────────────
NATO = {
    'a':"Alpha",'b':"Bravo",'c':"Charlie",'d':"Delta",'e':"Echo",'f':"Foxtrot",
    'g':"Golf",'h':"Hotel",'i':"India",'j':"Juliett",'k':"Kilo",'l':"Lima",
    'm':"Mike",'n':"November",'o':"Oscar",'p':"Papa",'q':"Quebec",'r':"Romeo",
    's':"Sierra",'t':"Tango",'u':"Uniform",'v':"Victor",'w':"Whiskey",'x':"X-ray",
    'y':"Yankee",'z':"Zulu",
}
PUNCT = {  # FLAG: ham/CW alternatives may be preferred ("stroke", "break", <AR>, <BT>)
    '.':"full stop", ',':"comma", ':':"colon", '-':"dash", '/':"slash",
    '=':"equals", '?':"question mark", '@':"at sign", '+':"plus",
}
UMLAUT = {'ä':"a umlaut", 'ö':"o umlaut", 'ü':"u umlaut"}
# Prosigns: firmware encodes them as single UPPERCASE chars in CWchars
# (cleanUpProSigns: S->'<as>' A->'<ka>' N->'<kn>' K->'<sk>' E->'<ve>' B->'<bk>' H->'ch').
# Spoken as "pro sign" + the two phonetic letters (composed from atoms).
PROSIGN_LETTERS = {'S':"as", 'A':"ka", 'N':"kn", 'K':"sk", 'E':"ve", 'B':"bk"}

# ── Spoken overrides for cryptic menu / action labels (firmware menu still flat;
#    the firmware-side spoken field for the menu lands in Phase 3). ────────────
MENU_SPOKEN = {
    "CW Abbrevs":   "CW abbreviations",
    "Learn New Chr":"Learn new character",
    "iCW/Ext Trx":  "Internet CW, external transceiver",
    "Adapt. Rand.": "Adaptive random",
    "Disp MAC Addr":"Display MAC address",
    "Config WiFi":  "Configure WiFi",
    "Update Firmw": "Update firmware",
    "Wifi Select":  "Select WiFi network",
}
ACTION_SPOKEN = {  # polished display label -> spoken
    "Calibrate Batt":"Calibrate battery", "Calibr. Batt.":"Calibrate battery",
    "Hardware Conf":"Hardware configuration",
}
VALUE_SPOKEN = {"+": "plus"}   # symbol-only option value (BLT <AR>) -> spoken word
UNIT_WORDS = ["words per minute", "Volume", "char", "Snapshot", "millivolts", "pro sign", "error"]


def strip_comments(t):
    t = re.sub(r"/\*.*?\*/", "", t, flags=re.S)
    return re.sub(r"//[^\n]*", "", t)

def preprocess(text, macros):
    out, stack = [], []
    act = lambda: all(f["a"] for f in stack) if stack else True
    for line in text.splitlines():
        s = line.strip()
        m = re.match(r"#\s*ifdef\s+(\w+)", s)
        if m: c = m.group(1) in macros; stack.append({"a":c,"t":c}); continue
        m = re.match(r"#\s*ifndef\s+(\w+)", s)
        if m: c = m.group(1) not in macros; stack.append({"a":c,"t":c}); continue
        if re.match(r"#\s*if\b", s): stack.append({"a":True,"t":True}); continue
        if re.match(r"#\s*else\b", s):
            if stack: f=stack[-1]; f["a"]=not f["t"]; f["t"]=True
            continue
        if re.match(r"#\s*endif\b", s):
            if stack: stack.pop()
            continue
        if act(): out.append(line)
    return "\n".join(out)

def array_body(text, decl):
    m = re.search(decl, text)
    if not m: raise RuntimeError("not found: " + decl)
    i = text.index("{", m.end()-1); depth=0; j=i
    while j < len(text):
        if text[j]=="{": depth+=1
        elif text[j]=="}":
            depth-=1
            if depth==0: return text[i+1:j]
        j+=1
    raise RuntimeError("unbalanced: "+decl)

def top_entries(body):
    out=[]; depth=0; start=None
    for k,ch in enumerate(body):
        if ch=="{":
            if depth==0: start=k+1
            depth+=1
        elif ch=="}":
            depth-=1
            if depth==0: out.append(body[start:k])
    return out

def load(name):
    with open(os.path.join(SRC, name), encoding="utf-8") as f: return f.read()

def slug(text):
    # Map '%' -> 'pct' so percentage option values ("25%") don't collide with the
    # bare-integer composition atoms ("25"); keeps slugs deterministic + lossless.
    t = text.lower().replace("%", "pct")
    s = re.sub(r"[^a-z0-9_]", "", t.replace(" ", "_"))
    return re.sub(r"_+", "_", s).strip("_")


# ── 1) Menu entries ──────────────────────────────────────────────────────────
menu_body = preprocess(array_body(strip_comments(load("MorseMenu.cpp")),
                                  r"menuText\s*\[\s*menuN\s*\]\s*="), POCKET_MACROS)
menu_entries = [s for s in QSTRING.findall(menu_body) if s.strip()]

# ── 2) Preferences: spokenName (else parName) + option values ────────────────
pl_body = preprocess(array_body(strip_comments(load("MorsePreferences.cpp")),
                                r"pliste\s*\[\s*\]\s*="), POCKET_MACROS)
pref_labels, option_values = [], []
for entry in top_entries(pl_body):
    b = entry.index("{")                       # the mapping{} brace (only brace in an entry)
    depth=0
    for k in range(b, len(entry)):
        if entry[k]=="{": depth+=1
        elif entry[k]=="}":
            depth-=1
            if depth==0: close=k; break
    before, inside, after = entry[:b], entry[b+1:close], entry[close+1:]
    names = QSTRING.findall(before)            # [parName, parDescript]
    spoken = QSTRING.findall(after)            # [spokenName] or []
    par = names[0] if names else ""
    pref_labels.append(spoken[0] if spoken else par)
    option_values += [VALUE_SPOKEN.get(v, v) for v in QSTRING.findall(inside) if v.strip()]

# ── 3) Action items (polished labels; spoken override where cryptic) ─────────
action_items = ["Select Lesson","Recall Snapshot","Store Snapshot","Calibrate Batt",
    "Hardware Conf","Call Sign","Op Name","Reset Scores","clear all","Cancel Recall",
    "Cancel Store","NO SNAPSHOTS","Flip Screen","Reset Defaults","Cancel","(not set)"]

# ── Assemble PHRASES (apply menu/action spoken overrides) ────────────────────
def spoken_of(s, table): return table.get(s, s)
phrase_texts = (
    [spoken_of(s, MENU_SPOKEN) for s in menu_entries] +
    pref_labels +
    option_values +
    [spoken_of(s, ACTION_SPOKEN) for s in action_items] +
    UNIT_WORDS
)

# ── ATOMS: NATO letters, punctuation, numbers ────────────────────────────────
letters   = list(NATO.values())                                    # Alpha..Zulu
punct     = list(PUNCT.values()) + list(UMLAUT.values()) + ["C H"]  # +ch (FLAG)
ints      = [str(i) for i in range(0,61)] + [str(i) for i in range(65,251,5)]
atom_texts = letters + punct + ints

# ── Character -> clip-sequence manifest (drives composition on-device) ───────
CWchars = "abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+SANKEBäöüH"
char_seq = {}
missing = []
for ch in CWchars:
    if ch in NATO:                       # letter
        char_seq[ch] = [slug(NATO[ch])]
    elif ch.isdigit():                   # digit -> number atom
        char_seq[ch] = [slug(ch)]
    elif ch in PUNCT:
        char_seq[ch] = [slug(PUNCT[ch])]
    elif ch in PROSIGN_LETTERS:          # prosign code -> "pro sign" + 2 phonetics
        a,b = PROSIGN_LETTERS[ch]
        char_seq[ch] = [slug("pro sign"), slug(NATO[a]), slug(NATO[b])]
    elif ch in UMLAUT:
        char_seq[ch] = [slug(UMLAUT[ch])]
    elif ch == 'H':                      # 'ch' digraph
        char_seq[ch] = [slug("C H")]
    else:
        missing.append(ch)
char_seq["<err>"] = [slug("pro sign"), slug("error")]

# ── Dedupe, collision check, write ───────────────────────────────────────────
all_texts = sorted({t for t in (phrase_texts + atom_texts) if t and t.strip()},
                   key=lambda s: s.lower())
slug_map = {}
for t in all_texts: slug_map.setdefault(slug(t), []).append(t)
collisions = {k:v for k,v in slug_map.items() if len(v) > 1}
empties    = slug_map.get("", [])

with open(os.path.join(HERE, "voice_strings.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(all_texts) + "\n")

manifest = {
    "phrases": {t: slug(t) for t in sorted(set(phrase_texts)) if t.strip()},
    "characters": char_seq,
    "collisions": collisions,
    "empty_slugs": empties,
    "counts": {
        "menu": len(set(menu_entries)), "pref_labels": len(set(pref_labels)),
        "option_values": len(set(option_values)), "actions": len(set(action_items)),
        "phrases_total": len({slug(t) for t in phrase_texts if t.strip()}),
        "atoms_total": len({slug(t) for t in atom_texts}),
        "clips_total": len({slug(t) for t in all_texts}),
    },
}
with open(os.path.join(HERE, "voice_manifest.json"), "w", encoding="utf-8") as f:
    json.dump(manifest, f, indent=2, ensure_ascii=False)

print("M32 Pocket voice-clip extraction")
print("="*40)
for k,v in manifest["counts"].items(): print(f"  {k:<16} {v:>4}")
print(f"  slug collisions  {len(collisions):>4}  {list(collisions.values())}")
print(f"  empty slugs      {len(empties):>4}  {empties}")
if missing: print(f"  !! chars with no voicing: {missing}")
print("wrote voice_strings.txt + voice_manifest.json")
