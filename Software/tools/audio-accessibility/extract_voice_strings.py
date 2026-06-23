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
import hashlib, json, os, re, sys

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

def clip_id(text):
    # Short, stable, filesystem-safe id: first 8 hex of md5(text). SPIFFS caps the
    # full path at 32 chars, so clips are stored as /voice/<id>.mp3 and the firmware
    # resolves UI string / character -> id via voice_manifest.json (no on-device slugify).
    return hashlib.md5(text.encode()).hexdigest()[:8]


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
char_seq = {}   # char -> ordered list of clip TEXTS (converted to ids in the manifest)
missing = []
for ch in CWchars:
    if ch in NATO:                       # letter
        char_seq[ch] = [NATO[ch]]
    elif ch.isdigit():                   # digit -> number atom
        char_seq[ch] = [ch]
    elif ch in PUNCT:
        char_seq[ch] = [PUNCT[ch]]
    elif ch in PROSIGN_LETTERS:          # prosign code -> "pro sign" + 2 phonetics
        a,b = PROSIGN_LETTERS[ch]
        char_seq[ch] = ["pro sign", NATO[a], NATO[b]]
    elif ch in UMLAUT:
        char_seq[ch] = [UMLAUT[ch]]
    elif ch == 'H':                      # 'ch' digraph
        char_seq[ch] = ["C H"]
    else:
        missing.append(ch)
char_seq["<err>"] = ["pro sign", "error"]

# ── Dedupe, assign ids, write ────────────────────────────────────────────────
all_texts = sorted({t for t in (phrase_texts + atom_texts) if t and t.strip()},
                   key=lambda s: s.lower())
id_map = {}
for t in all_texts: id_map.setdefault(clip_id(t), []).append(t)
collisions = {k: v for k, v in id_map.items() if len(v) > 1}   # md5 collisions (expect none)

with open(os.path.join(HERE, "voice_strings.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(all_texts) + "\n")

manifest = {
    "voice_dir": "/voice",
    "phrases": {t: clip_id(t) for t in sorted(set(phrase_texts)) if t.strip()},
    "characters": {c: [clip_id(t) for t in seq] for c, seq in char_seq.items()},
    "clips": {clip_id(t): t for t in all_texts},   # id -> text (firmware reverse map / debug)
    "id_collisions": collisions,
    "counts": {
        "menu": len(set(menu_entries)), "pref_labels": len(set(pref_labels)),
        "option_values": len(set(option_values)), "actions": len(set(action_items)),
        "phrases_total": len({clip_id(t) for t in phrase_texts if t.strip()}),
        "atoms_total": len({clip_id(t) for t in atom_texts}),
        "clips_total": len(all_texts),
    },
}
with open(os.path.join(HERE, "voice_manifest.json"), "w", encoding="utf-8") as f:
    json.dump(manifest, f, indent=2, ensure_ascii=False)

# ── Emit voice_clips.h for the firmware: firmware-facing UI string -> clip id ──
# The firmware announces using the strings it actually holds: menu entries by their
# DISPLAY text (menuText[]), prefs by spokenName||parName, option values + numbers by
# their displayed text. So we key the lookup by those firmware-facing strings, mapping
# each to the id of its (possibly spoken-override) clip.
fw_lookup = {}
def fw_add(key, clip_text):
    if key and key.strip():
        fw_lookup[key] = clip_text
for s in menu_entries:  fw_add(s, spoken_of(s, MENU_SPOKEN))   # display -> spoken clip
for lbl in pref_labels: fw_add(lbl, lbl)
for v in option_values: fw_add(v, v)
for s in action_items:  fw_add(s, spoken_of(s, ACTION_SPOKEN))
for t in UNIT_WORDS + ints + letters + punct: fw_add(t, t)     # numbers/letters announce by own text

def cstr(s): return s.replace("\\", "\\\\").replace('"', '\\"')
HDR = os.path.join(SRC, "voice_clips.h")
with open(HDR, "w", encoding="utf-8") as f:
    f.write("// voice_clips.h - GENERATED by Software/tools/audio-accessibility/extract_voice_strings.py\n")
    f.write("// Do not edit by hand. Maps a firmware-facing UI string -> SPIFFS clip id (/voice/<id>.mp3).\n")
    f.write("// Sorted by strcmp() byte order for binary search (see MorseVoice::announce).\n")
    f.write("#ifndef VOICE_CLIPS_H_\n#define VOICE_CLIPS_H_\n\n")
    f.write("struct VoiceEntry { const char* key; const char* id; };\n\n")
    f.write("static const VoiceEntry voiceLookup[] = {\n")
    for key in sorted(fw_lookup):                                  # code-point order == strcmp for ASCII
        f.write(f'  {{"{cstr(key)}", "{clip_id(fw_lookup[key])}"}},\n')
    f.write("};\n")
    f.write(f"static const unsigned int voiceLookupCount = {len(fw_lookup)};\n\n")
    f.write("#endif // VOICE_CLIPS_H_\n")

print("M32 Pocket voice-clip extraction")
print("="*40)
for k,v in manifest["counts"].items(): print(f"  {k:<16} {v:>4}")
print(f"  md5 id collisions {len(collisions):>3}  {list(collisions.values())}")
if missing: print(f"  !! chars with no voicing: {missing}")
print(f"wrote voice_strings.txt + voice_manifest.json + voice_clips.h ({len(fw_lookup)} fw keys)")
