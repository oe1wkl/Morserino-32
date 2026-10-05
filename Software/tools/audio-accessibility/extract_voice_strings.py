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

Reads the firmware's own tables, as the Accessibility Edition (pocketwroom-accessibility)
compiles them, so the set regenerates when entries change. Outputs (next to this script):
  voice_strings.txt   all distinct clip texts, deduped + sorted  (feeds generate_audio.sh)
  voice_manifest.json phrases{text:slug}, characters{char:[slug,...]}, collisions, counts

Decisions (maintainer, 2026-06-23): spoken label = dedicated field; letters = NATO
phonetic; prosigns = "pro sign" + phonetic letters composed from atoms.
"""
import hashlib, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.environ.get("M32_SRC", os.path.normpath(
    os.path.join(HERE, "..", "..", "src", "Version 6 and newer")))

# The tables are evaluated as the ACCESSIBILITY EDITION compiles them -- the only build that
# plays these clips. The macro set is read from that env's build_flags in platformio.ini, not
# hand-listed: a hand list (the standard Pocket's, until 2026-10) rendered clips for code the
# a11y build never compiles -- the games, Font Size, Upload File / Update Firmw -- spending the
# nearly full voice store on entries a blind operator can never reach.
A11Y_ENV = os.environ.get("M32_A11Y_ENV", "pocketwroom-accessibility")
PIO_INI = os.path.normpath(os.path.join(SRC, "..", "platformio.ini"))

def pio_macros(ini_path, env):
    """-D macros of [env:<env>]: follows `extends`, expands ${section.key}, applies build_unflags."""
    sections, cur = {}, None
    with open(ini_path, encoding="utf-8") as f:
        for raw in f:
            line = re.sub(r"(^|\s);.*$", "", raw.rstrip("\n"))      # full-line and inline ; comments
            m = re.match(r"\s*\[([^\]]+)\]\s*$", line)
            if m:
                cur = m.group(1).strip(); sections[cur] = {}; key = None; continue
            if cur is None or not line.strip():
                continue
            m = re.match(r"([A-Za-z_][\w.]*)\s*=(.*)$", line)
            if m and not line[0].isspace():
                key = m.group(1); sections[cur][key] = [m.group(2).strip()]
            elif key:
                sections[cur][key].append(line.strip())             # continuation line

    def value(sec, key, seen=()):
        if (sec, key) in seen: raise RuntimeError(f"platformio.ini: ${{{sec}.{key}}} is circular")
        s = sections.get(sec, {})
        if key not in s:
            ext = s.get("extends")
            if ext: return value(ext[0].strip(), key, seen + ((sec, key),))
            return []
        out = []
        for tok in s[key]:
            m = re.fullmatch(r"\$\{([^.}]+)\.([^}]+)\}", tok)
            out += value(m.group(1), m.group(2), seen + ((sec, key),)) if m else [tok]
        return out

    sec = "env:" + env
    if sec not in sections: raise RuntimeError(f"{ini_path}: no [{sec}]")
    def defs(lines):
        return {m.group(1) for l in lines for m in re.finditer(r"-D\s*(\w+)", l)}
    return defs(value(sec, "build_flags")) - defs(value(sec, "build_unflags"))

A11Y_MACROS = pio_macros(PIO_INI, A11Y_ENV)
if "CONFIG_AUDIO_A11Y" not in A11Y_MACROS:
    sys.exit(f"{PIO_INI} [env:{A11Y_ENV}] does not define CONFIG_AUDIO_A11Y - wrong env?")
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
# The firmware's generator codes for these (CWchars[] in m32_v6.ino): one byte, the Latin-1 value -
# what the Practice Set picker and Koch Custom Chars hand to announceMoreChar(). The other national
# letters (é è à ç ñ å æ ø) only exist where a Decoder Chars set offers them, and that preference is
# not in the Accessibility Edition: left unvoiced by decision (2026-10-02), so no clips.
CODE_OF = {'ä': '\xe4', 'ö': '\xf6', 'ü': '\xfc'}
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
    # The remote rig's settings (D17). Display labels are 12-char abbreviations carrying a "Rig:" prefix so they
    # cannot be mistaken for local ones; read out as written they would be "Rig colon Lim Kyr".
    "Rig: Delay":"Remote rig playout delay",
    "Rig: Lim Kyr":"Remote rig key down limit, keyer",
    "Rig: Lim SK":"Remote rig key down limit, straight key",
    "Rig: 1st Ext":"Remote rig first element extension",
    "Rig: Hang":"Remote rig hang time",
    "Pass Phrase":"Remote keying pass phrase",   # also the text-entry heading ("Pass Phrase:" minus the colon)
}
VALUE_SPOKEN = {"+": "plus"}   # symbol-only option value (BLT <AR>) -> spoken word
# "of" / "characters" join the composed value lines (see MorsePreferences::announceValue):
# "21 of 51" for the Koch lesson, "39 characters" for the practice set.
UNIT_WORDS = ["words per minute", "Volume", "char", "characters", "of", "Snapshot",
              "millivolts", "pro sign", "error",
              # Remote Rig hang time (D16): "2 thousand 3 hundred 50 milliseconds", "7 and a half dits"
              "milliseconds", "dits", "and a half", "thousand", "hundred"]
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
# Remote keying pass phrase entry (editKipPassPhrase() in MorsePreferences.cpp): the three outcomes, drawn on
# screen, not table-driven. "Too short" is followed by the composed "12 characters" (existing atoms).
PASSPHRASE_WORDS = ["Unchanged", "Too short", "Phrase saved"]
# BLE Serial splashes (MorseBleSerial.cpp: bleInitFail(), suspendForWifi()). Drawn, not table-driven,
# and the protocol text stream cannot carry them: they announce the end, or the failed start, of the
# very session that stream would need.
BLE_NOTICE_WORDS = ["Bluetooth serial failed to start", "Bluetooth serial suspended for wireless mode"]
# Text entry (MorseTextEntry, voiceAsLetters): the space in Op Name is spoken as the word.
TEXT_ENTRY_WORDS = ["space"]
# Remote Keyer status (MorseKipKeyer.cpp, TODO B8): drawn, not table-driven. Short on purpose - the voice
# store is nearly full - and spoken as one sentence where the display breaks a hint over two lines.
KIP_KEYER_WORDS = ["Connecting", "Calling rig", "Linked to rig", "No pass phrase set", "No rig address set",
                   "Rig address not found", "No answer from the rig", "Out of memory",
                   "Link lost", "Link restored"]          # link changes while in the mode (idle gaps only)

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
        m = re.match(r"#\s*if\s+(!?)\s*defined\s*\(?\s*(\w+)\s*\)?\s*$", s)   # #if [!]defined(X)
        if m: c = (m.group(2) in macros) != bool(m.group(1)); stack.append({"a":c,"t":c}); continue
        if re.match(r"#\s*(if|elif)\b", s):
            # Anything richer would be guessed, and a wrong guess is a clip for code the a11y
            # build drops (wasted store) or no clip for code it keeps (silence). Refuse instead.
            raise RuntimeError("extend preprocess() for this conditional: " + s)
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
                                  r"menuText\s*\[\s*menuN\s*\]\s*="), A11Y_MACROS)
menu_entries = [s for s in QSTRING.findall(menu_body) if s.strip()]

# ── 2) Preferences: spokenName (else parName) + option values ────────────────
pl_body = preprocess(array_body(strip_comments(load("MorsePreferences.cpp")),
                                r"pliste\s*\[\s*\]\s*="), A11Y_MACROS)
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
                                   r"extraItems\s*\[\s*\]\s*="), A11Y_MACROS)
extra_items = [s for s in QSTRING.findall(extra_body) if s.strip()]
inline_values = ["clear all","Cancel Recall","Cancel Store","NO SNAPSHOTS",
    "Flip Screen","Reset Defaults","Cancel","(not set)","(set)"]
action_items = extra_items + inline_values

# ── Assemble PHRASES (apply menu/action spoken overrides) ────────────────────
def spoken_of(s, table): return table.get(s, s)
phrase_texts = (
    [spoken_of(s, MENU_SPOKEN) for s in menu_entries] +
    pref_labels +
    option_values +
    [spoken_of(s, ACTION_SPOKEN) for s in action_items] +
    UNIT_WORDS + SPLASH_WORDS + CONSENT_WORDS + KOCH_FALLBACK_WORDS + PASSPHRASE_WORDS + BLE_NOTICE_WORDS + TEXT_ENTRY_WORDS + KIP_KEYER_WORDS
)
# NOTE: a non-table word list must appear TWICE -- here, which schedules the clip for
# rendering, and in the fw_add() loop below, which maps the firmware string to that clip.
# Only one of the two and the string is silent with no error anywhere: voice_clips.h names
# a clip that generate_audio.sh was never asked to render.

# ── ATOMS: NATO letters, punctuation, numbers ────────────────────────────────
letters   = list(NATO.values())                                    # Alpha..Zulu
punct     = list(PUNCT.values()) + list(UMLAUT.values()) + ["C H"]  # +ch (FLAG)
ints      = [str(i) for i in range(0,61)] + [str(i) for i in range(65,251,5)]
# Larger numbers are COMPOSED ("2 thousand 3 hundred 50"), not stored: the SPIFFS voice store has little headroom
# left. One clip per value 300-3000 in 50s (tried for the Remote Rig hang time, D16) overflowed it by ~520 KB.
atom_texts = letters + punct + ints

# ── Character -> clip-sequence manifest (drives composition on-device) ───────
CWchars = "abcdefghijklmnopqrstuvwxyz0123456789.,:-/=?@+SANKEB" + "".join(CODE_OF.values()) + "H"
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
    elif ch in CODE_OF.values():          # ä ö ü as their one-byte generator code
        char_seq[ch] = [UMLAUT[next(k for k, v in CODE_OF.items() if v == ch)]]
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
for t in UNIT_WORDS + SPLASH_WORDS + CONSENT_WORDS + KOCH_FALLBACK_WORDS + PASSPHRASE_WORDS + BLE_NOTICE_WORDS + TEXT_ENTRY_WORDS + KIP_KEYER_WORDS + ints + letters + punct: fw_add(t, t)  # announce by own text

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
        key = "".join(f"\\x{ord(x):02X}" if ord(x) > 0x7F else cstr(x) for x in ch)   # code bytes as escapes
        f.write(f'  {{"{key}", {len(ids)}, {{{slots}}}}},\n')
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
