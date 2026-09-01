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
    "CONFIG_BLUETOOTH_KEYBOARD", "CONFIG_BLE_SERIAL", "LORA_DISABLED", "CONFIG_ENGLISH_OXFORD",
    "CONFIG_TLV320AIC3100", "CONFIG_MCP73871", "CONFIG_DECODER_I2S",
    "CONFIG_BATMEAS_PIN", "ARDUINO_USB_MODE", "ARDUINO_USB_CDC_ON_BOOT",
    "CONFIG_PRACTICE_STATS",
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
ACTION_SPOKEN = {  # extraItems[] display label -> spoken
    "Calibrate Batt":"Calibrate battery", "Calibr. Batt.":"Calibrate battery",
    "Hardware Conf":"Hardware configuration",
    "RECALLSnapshot":"Recall snapshot",   # display label runs the two words together
    "STORE Snapshot":"Store snapshot",
    "Koch Lesson":"Koch lesson",
    "LoRa Frequ":"LoRa frequency",
}
VALUE_SPOKEN = {"+": "plus"}   # symbol-only option value (BLT <AR>) -> spoken word
# "of" / "characters" join the composed value lines (see MorsePreferences::announceValue):
# "21 of 51" for the Koch lesson, "39 characters" for the practice set.
UNIT_WORDS = ["words per minute", "Volume", "char", "characters", "of", "Snapshot",
              "millivolts", "pro sign", "error"]
# Boot splash (announceSplash() in m32_v6.ino). The splash is drawn, not table-driven, so
# these phrases live nowhere the extractor could find them and are listed here instead.
# Version number and battery voltage are composed from the number atoms below ("version" +
# "9" + "point" + "0" + "beta"), so neither a version bump nor a new reading needs a clip.
SPLASH_WORDS = ["Morserino 32 accessibility edition", "version", "point", "beta",
                "battery", "volts", "battery empty"]
# BLE Serial consent prompt (bleConsentPrompt() in m32_v6.ino) -- see
# devdocs/ble-serial/ACCESS_CONTROL.md. Drawn, not table-driven, and it cannot use the
# protocol text stream that CLAUDE.md §8 case 2 prescribes: the protocol session is
# precisely what the operator is being asked to authorize, so it does not exist yet.
# Without these clips the prompt is a lock a blind operator cannot open.
CONSENT_WORDS = ["Allow Bluetooth connection? F N for yes, click for no.",
                 "Connection allowed.", "Connection refused."]
# Koch Sequence -> Custom Chars with no usable /player.txt (adjustKeyerPreference() in
# MorsePreferences.cpp). Drawn, not table-driven. Without these the operator selects Custom
# Chars, hears nothing at all for the ~2 s the two messages are up, and is then told the value
# is "M32" - which reads as the encoder having slipped rather than as a missing character set.
# The sequence it falls back to is spoken from the Koch Sequence option clips, so only the two
# fixed phrases are needed here.
KOCH_FALLBACK_WORDS = ["No custom set", "Fallback"]

# User-editable pronunciation overrides (spoken_overrides.tsv): firmware string -> spoken text.
# Highest priority -- lets the maintainer hand-tune how any entry / option / label is pronounced.
USER_OVERRIDES = {}
_ovr = os.path.join(HERE, "spoken_overrides.tsv")
if os.path.exists(_ovr):
    with open(_ovr, encoding="utf-8") as _f:
        for _line in _f:
            _line = _line.rstrip("\n")
            if not _line or _line.lstrip().startswith("#") or "\t" not in _line:
                continue
            _k, _v = _line.split("\t", 1)
            if _k.strip() and _v.strip():
                USER_OVERRIDES[_k.strip()] = _v.strip()


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

# ── 3) Action items: the firmware's own extraItems[] (spoken override where the
#      12-char display label is cryptic), plus the fixed value strings that
#      getValueLine() builds inline and that live in no table. ────────────────
# Read from the firmware rather than hand-listed: a hardcoded list silently drifted
# ("Recall Snapshot" vs the real "RECALLSnapshot"), so those headings had no clip
# under the string the firmware actually announces.
extra_body = preprocess(array_body(strip_comments(load("MorsePreferences.cpp")),
                                   r"extraItems\s*\[\s*\]\s*="), POCKET_MACROS)
extra_items = [s for s in QSTRING.findall(extra_body) if s.strip()]
inline_values = ["clear all","Cancel Recall","Cancel Store","NO SNAPSHOTS",
    "Flip Screen","Reset Defaults","Cancel","(not set)"]
action_items = extra_items + inline_values

# ── Assemble PHRASES (apply menu/action spoken overrides) ────────────────────
def spoken_of(s, table): return table.get(s, s)
phrase_texts = (
    [spoken_of(s, MENU_SPOKEN) for s in menu_entries] +
    pref_labels +
    option_values +
    [spoken_of(s, ACTION_SPOKEN) for s in action_items] +
    UNIT_WORDS + SPLASH_WORDS + CONSENT_WORDS + KOCH_FALLBACK_WORDS
)
# NOTE: a non-table word list must appear TWICE -- here, which schedules the clip for
# rendering, and in the fw_add() loop below, which maps the firmware string to that clip.
# Only one of the two and the string is silent with no error anywhere: voice_clips.h names
# a clip that generate_audio.sh was never asked to render.

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
maxseq = max(len(v) for v in char_seq.values())   # C array width for voiceCharLookup[]

# ── Dedupe, assign ids, write ────────────────────────────────────────────────
# Tiebreak on the exact string: the input is a set (iteration order varies between
# runs), so a bare .lower() key left case-only pairs -- "Adaptive Random" vs
# "Adaptive random" -- swapping places on every run. That churn made the "empty
# diff = nothing owed" check in CLAUDE.md section 8 unusable.
all_texts = sorted({t for t in (phrase_texts + atom_texts + list(USER_OVERRIDES.values()))
                    if t and t.strip()}, key=lambda s: (s.lower(), s))
id_map = {}
for t in all_texts: id_map.setdefault(clip_id(t), []).append(t)
collisions = {k: v for k, v in id_map.items() if len(v) > 1}   # md5 collisions (expect none)

# ── Pack stamp: which clip set is this? ──────────────────────────────────────
# Clip names are content hashes, so a firmware whose strings changed looks for ids that
# an older pack simply does not contain -- and the device goes quiet on exactly those
# entries, with nothing to show for it. The stamp lets the firmware notice: it is
# compiled in (VOICE_PACK_STAMP below) and written into the pack itself as
# /voice/pack.txt by generate_audio.sh, which compares them at boot.
#
# The rule is fixed so the shell script can reproduce it without parsing anything:
#     first 8 hex of md5( each unique clip id, sorted, one per line, trailing newline )
# i.e. exactly `LC_ALL=C sort -u <ids> | md5`. Keep the two in step (generate_audio.sh).
PACK_STAMP = hashlib.md5(
    ("\n".join(sorted(id_map)) + "\n").encode("utf-8")).hexdigest()[:8]

with open(os.path.join(HERE, "voice_strings.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(all_texts) + "\n")

manifest = {
    "voice_dir": "/voice",
    "phrases": {t: clip_id(t) for t in sorted(set(phrase_texts)) if t.strip()},
    "characters": {c: [clip_id(t) for t in seq] for c, seq in char_seq.items()},
    "clips": {clip_id(t): t for t in all_texts},   # id -> text (firmware reverse map / debug)
    "id_collisions": collisions,
    "pack_stamp": PACK_STAMP,
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
        fw_lookup[key] = USER_OVERRIDES.get(key, clip_text)   # user override wins
for s in menu_entries:  fw_add(s, spoken_of(s, MENU_SPOKEN))   # display -> spoken clip
for lbl in pref_labels: fw_add(lbl, lbl)
for v in option_values: fw_add(v, v)
for s in action_items:  fw_add(s, spoken_of(s, ACTION_SPOKEN))
for t in UNIT_WORDS + SPLASH_WORDS + CONSENT_WORDS + KOCH_FALLBACK_WORDS + ints + letters + punct: fw_add(t, t)  # announce by own text

def cstr(s): return s.replace("\\", "\\\\").replace('"', '\\"')
HDR = os.path.join(SRC, "voice_clips.h")
with open(HDR, "w", encoding="utf-8") as f:
    f.write("// voice_clips.h - GENERATED by Software/tools/audio-accessibility/extract_voice_strings.py\n")
    f.write("// Do not edit by hand. Maps a firmware-facing UI string -> SPIFFS clip id (/voice/<id>.mp3).\n")
    f.write("// Sorted by strcmp() byte order for binary search (see MorseVoice::announce).\n")
    f.write("#ifndef VOICE_CLIPS_H_\n#define VOICE_CLIPS_H_\n\n")
    f.write("// Identifies the clip set this firmware expects. generate_audio.sh writes the same\n")
    f.write("// value into the pack as /voice/pack.txt; MorseVoice::clipStoreOk() compares them at\n")
    f.write("// boot, so a pack left over from an older firmware is reported instead of silently\n")
    f.write("// missing whichever clips changed.\n")
    f.write(f'#define VOICE_PACK_STAMP "{PACK_STAMP}"\n\n')
    f.write("struct VoiceEntry { const char* key; const char* id; };\n\n")
    f.write("static const VoiceEntry voiceLookup[] = {\n")
    for key in sorted(fw_lookup):                                  # code-point order == strcmp for ASCII
        f.write(f'  {{"{cstr(key)}", "{clip_id(fw_lookup[key])}"}},\n')
    f.write("};\n")
    f.write(f"static const unsigned int voiceLookupCount = {len(fw_lookup)};\n\n")

    # Character -> clip SEQUENCE (MorseVoice::announceMoreChar). Keyed by the RAW
    # firmware character, i.e. before cleanUpProSigns(): the uppercase prosign codes
    # ('S','A','N','K','E','B','H') map to "pro sign" + two phonetics, everything else
    # to a single atom. Unsorted (56 entries, linear scan once per encoder detent).
    f.write(f"struct VoiceCharEntry {{ const char* key; unsigned char n; const char* ids[{maxseq}]; }};\n\n")
    f.write("static const VoiceCharEntry voiceCharLookup[] = {\n")
    for ch in sorted(char_seq):
        ids = [clip_id(t) for t in char_seq[ch]]
        slots = ", ".join(f'"{i}"' for i in ids) + ", nullptr" * (maxseq - len(ids))
        f.write(f'  {{"{cstr(ch)}", {len(ids)}, {{{slots}}}}},\n')
    f.write("};\n")
    f.write(f"static const unsigned int voiceCharLookupCount = {len(char_seq)};\n\n")
    f.write("#endif // VOICE_CLIPS_H_\n")

print("M32 Pocket voice-clip extraction")
print("="*40)
for k,v in manifest["counts"].items(): print(f"  {k:<16} {v:>4}")
print(f"  md5 id collisions {len(collisions):>3}  {list(collisions.values())}")
if missing: print(f"  !! chars with no voicing: {missing}")
print(f"  pack stamp        {PACK_STAMP}")
print(f"wrote voice_strings.txt + voice_manifest.json + voice_clips.h ({len(fw_lookup)} fw keys)")
