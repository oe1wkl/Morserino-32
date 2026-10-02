# Decoder: ITU brackets and national characters — investigation

*2026-10-02. Prompted by two tester requests (TODO I3): decoding per ITU-R M.1677-1, and characters for
French, Spanish, Portuguese and the Nordic languages. **Decided and implemented the same day — see
"Implementation" at the end;** the investigation below is kept as written.*

## Decisions already taken (Willi, 2026-10-02)

- **`-.--.` stays `<kn>` by default** (amateur practice). A decoder preference may switch it to `(` (ITU).
- **The Koch character sets are not touched** — they are established in courses and schools.
- **Decoder-only options are left out of the Accessibility Edition**: decoded text is shown on screen, which a blind
  operator does not use, and every new preference costs voice clips the a11y store can hardly afford (~49 KB left).
  The same rule applies to any language option.

## What the decoder knows today

`CWtree[]` in `MorseDecoder.h` already decodes every ITU-R M.1677-1 punctuation mark except the brackets —
`. , : ? ' - / " = + @` (plus `; !`) — the prosigns, and the German extras **ä ö ü ch**. Gaps that matter here:

| Code | Today | ITU-R M.1677-1 | National use |
|---|---|---|---|
| `-.--.`  | `<kn>` | `(` | — |
| `-.--.-` | `*` (no node) | `)` | — |
| `..-..`  | `*` (no node) | **é** | French |
| `.-..-`  | `*` | — | **è** (French), ł |
| `-.-..`  | `*` (no node) | — | **ç** (French, Portuguese) |
| `--.--`  | `*` (no node) | — | **ñ** (Spanish) |
| `.--.-`  | `*` | — | **à** (French, Portuguese) = **å** (Swedish, Danish, Norwegian) |
| `.-.-`   | `ä` | — | ä (German, Swedish) = **æ** (Danish, Norwegian) |
| `---.`   | `ö` | — | ö (German, Swedish) = **ø** (Danish, Norwegian) |
| `..--`   | `ü` | — | ü (German, also Spanish/Portuguese loanwords) |
| `----`   | `<ch>` | — | ch (German, historically Spanish) |

Codes and sharings as in the common references (e.g. morsecode.world, Wikipedia "Morse code", *Diacritics and
non-Latin extensions*); only é is in an ITU recommendation. Accented vowels other than é/è/à (á í ó ú â ê ô ã õ)
have no established codes and are normally sent without the accent.

**The design consequence:** `.--.-`, `.-.-` and `---.` mean different letters in different languages, so no single
tree is right for everybody — only a setting can say whether `.-.-` is ä or æ. That makes this a single selector,
not a set of toggles (Willi's rule for mutually exclusive features), and it can absorb the `(`/`<kn>` choice.

## Fonts — not the blocker

- **OLED** (`wklfonts.cpp`, DialogInput 12/15, plain and bold): all four contain every letter needed — checked
  glyph by glyph: à á â ã ä å æ ç è é ê í ñ ó ô õ ö ø ú ü.
- **M32 Pocket, normal scroll font** (IntelOneMono 12/15 pt, `*8b.h`): full Latin-1, 0x20–0xFF.
- **M32 Pocket, Font Size Small** (`IntelOneMono12ptScroll.h`): a hand-picked table — ASCII, ©, Ä Ö Ü ß ä ö ü only.
  The new letters (and their capitals, for Output Case UPPER) would have to be added: regenerate the table with more
  ranges, a few hundred bytes. Same procedure as the 2026-09 umlaut fix (memory note on its traps).
- The status line font (DejaVuSansMono 7b) is ASCII only, but decoded text never goes there.

All the new letters are in the UTF-8 `0xC3` block (à = C3 A0 … ø = C3 B8), where the existing upper-casing code
already works by offset; the display path already handles UTF-8.

## Other places the decoded text goes

- **Serial protocol / BLE Serial**: UTF-8 JSON — fine as is.
- **Bluetooth keyboard ("Decoded output", VBand+Decoded)**: a HID keyboard sends key codes, not characters, and
  what a key code types depends on the host's keyboard layout — à/å/æ/ç/ñ/ø cannot be typed reliably. Needs a
  decision: transliterate (å→aa, æ→ae, ø→oe, ç→c, ñ→n, é/è/à→e/e/a) for keyboard output, or send nothing.
- **Echo Trainer / Koch / generator**: unaffected — they compare against their own generated text, and the Koch
  sets stay as they are.

## Proposal (for decision)

One decoder preference, e.g. **"Decoder Chars"**, compiled out of the a11y edition, values:

| Value | `-.--.` | `.--.-` | `.-.-` | `---.` | extra |
|---|---|---|---|---|---|
| **Standard** (default = today) | `<kn>` | `*` | ä | ö | ü ch |
| **ITU** | `(` | `*` | `*`? | `*`? | `)` é |
| **French** | `<kn>` | à | ä | ö | é è ç |
| **Spanish/Port.** | `<kn>` | à | ä | ö | ñ ç é ü |
| **Nordic** | `<kn>` | å | ä→**æ** for Danish/Norwegian? | ö→**ø**? | — |

Open questions for Willi:
1. **Value list.** Is "Nordic" one value, or Swedish (å ä ö) vs Danish/Norwegian (å æ ø)? They differ exactly on
   the two shared codes, so it has to be two values to be right.
2. **Should `)`, é, è, ç, ñ simply be added to every value** where they do not conflict (they have no other
   meaning), leaving the selector only for the genuinely ambiguous codes and `(`? That keeps the list short.
3. **ITU value**: keep the German extras, or decode strictly to M.1677-1?
4. **Bluetooth keyboard output** of the new letters: transliterate, or drop?

Cost estimate once decided: a mapping layer on top of `CWtree[]` (new nodes for `-.--.-`, `..-..`, `-.-..`,
`--.--`; per-language symbols for the shared codes), one `prefPos` (three parallel arrays, CLAUDE.md §3 rule 10),
the small-font regeneration, EN+DE manuals, and fixing the voice extractor to model the a11y build (below).

## Prerequisite: the voice extractor models the wrong build

`extract_voice_strings.py` evaluates the firmware tables with the **standard** M32 Pocket macros
(`POCKET_MACROS`, which keep `CONFIG_CW_GAME`). A preference excluded from the a11y edition would therefore still get
its clips rendered into the a11y voice store — and the game strings already do: clips the a11y edition can never
play. Teaching the extractor the a11y configuration (`CONFIG_AUDIO_A11Y` on, `CONFIG_CW_GAME` and
`CONFIG_SCROLL_FONT_SIZE` off, as in `platformio.ini`) is needed before any a11y-excluded preference, and is likely
to give back voice-store space now.

## Decisions (Willi, 2026-10-02) — the four open questions

1. **Five values:** Standard · ITU · Fr/Es/Pt · Sv/Fi · Da/No. French, Spanish and Portuguese never clash, so one
   value; Swedish/Finnish and Danish/Norwegian disagree on exactly the two shared codes, so two.
2. **Standard is unchanged.** Each value adds only its own letters; Standard keeps `*` for unknown codes (a clear
   error sign for a learner).
3. **ITU = Standard + `(` `)` é**, keeping ä ö ü ch (ITU gives those codes no other meaning).
4. **Bluetooth keyboard: transliterate** (ä→ae ö→oe ü→ue å→aa æ→ae ø→oe à→a é/è→e ç→c ñ→n, capitals likewise) —
   which also changes ä ö ü, which used to type nothing.

## Implementation (branch `decoder-language`, 2026-10-02)

- **Tree** (`MorseDecoder.h`): four new nodes 69–72 (`-.--.-`, `..-..`, `-.-..`, `--.--`) that decode as `*`, so
  Standard is byte-for-byte the old decoder — checked on the host for all 254 codes of up to seven elements.
- **Substitution** (`decodedSymbol()`, `MorseDecoder.cpp`): a ten-row table {node, value mask, letter}; every
  reader of the tree goes through it (`M32MorseTable::retrieveSymbol()`, `CWwordToClearText()`).
- **Where it applies** (`decoderCharSet()`, `m32_v6.ino`): a positive list — CW Keyer, Decoder, LoRa/WiFi/iCW-Ext
  transceivers. Echo/Koch trainers, games (`gameMode`, incl. Pileup multiplayer receive) and the QSO Bot always get
  Standard: they compare keyed text with their own (ITU's `(` would mark a correct `<kn>` wrong; Da/No's æ a
  correct ä). Any future mode defaults to Standard. **When merging into `m32kip`:** decide whether the Remote
  Keyer/Rig modes join the list and their option lists get `DECCHARS`.
- **Preference** `posDecoderChars` / NVS key `decoderChars` (1 entry), in the Keyer, Decoder, the three
  transceiver lists and All; `#ifndef CONFIG_AUDIO_A11Y` throughout — the voice extractor (now modelling the a11y
  build) confirms nothing is owed. **Not in snapshots** (`storedInSnapshot()`): it is the operator's language, and a
  recalled or imported snapshot must not reset a Danish operator to Standard.
- **Upper case:** `toUpperCaseM32()` now capitalises every Latin-1 small letter (0xA0–0xBE except ÷).
- **Bluetooth keyboard** (`MorseBluetooth.cpp`): transliteration (`asciiForLatin1()`), and a latent bug fixed on the
  way — `keymap[chr]` indexed with a *signed* char and `val > KEYMAP_SIZE` let UTF-8 bytes 0x80–0x98 through, so
  with Output Case UPPER a decoded Ä (C3 **84**) or Ö (C3 **96**) typed an unrelated key.
- **Fonts:** OLED (all four) already had every letter and capital. The Pocket's Font Size Small table is now
  generated by `Software/tools/fonts/make_scroll_font.py`, with its own compacted bitmaps (119 glyphs; ~6.6 KB less
  flash than reusing the full in-tree copies). À È É Ñ were taller than the 20 px ascent budget and are squashed by
  one gap row (+ the accent's tip pixel on À È É), keeping the 27 px pitch and the fifth line; the 103 old glyphs are
  bit-identical, verified against the linked ELF symbol sizes.
- **Docs:** EN+DE V9 manual (row in "Transmitting, Decoding and QSO Bot", snapshot exclusion list, Bluetooth
  keyboard note — tagged classic + pocket), changelog V. 9.1 (+ what's-new, both languages), Configuration Tool
  help text.
- **Owed:** bench test on both variants — each value in CW Keyer and Decoder, Output Case UPPER, Font Size Small,
  a Trx pair, Bluetooth keyboard output; and one Echo Trainer round with ITU set (`<kn>` must still count as right).

## Phase 2 — the letters in user-defined training (branch `generator-national-chars`, 2026-10-02)

Willi's afterthought: should the file player, Custom Chars and the Practice Set allow the new characters too?
Key point: **letter → code is unambiguous; only code → letter needs the language.** So generation needs no setting,
and the Echo Trainer can judge by the target word. Decisions (Willi, all as recommended): do it; ä ö ü in files get
their own codes (no longer ae oe ue); the Practice Set picker offers the letters of the chosen Decoder Chars set
(Standard and the a11y edition: ä ö ü ch); the new letters stay unvoiced in the a11y edition for now.

- **One-byte codes.** The generator is byte-based (CWchars[] ↔ pool[] by byte index; Koch/custom/practice sets and the
  echo comparison by `char`). National letters now travel as their Latin-1 byte (ä = 0xE4 …), like prosigns travel
  as one capital letter. `CWchars[]` gained ä ö ü ch ( ) é è à ç ñ å æ ø at 51–64 and `pool[]` the matching
  entries, with a `static_assert` that the two stay in step. **Latent bug fixed on the way:** ä ö ü used to sit in
  CWchars as UTF-8 (two bytes each), so their byte positions no longer matched pool[] and `H` (ch) indexed past its
  end — harmless only because nothing could generate them.
- **Conversions at the edges** (m32_v6.ino): `foldLatin1()` is the one mapping (letters with a code → code; other
  accented letters → plain letter; ß → ss; capitals → small). In: `utf8umlaut()` (file text, hence also Custom Chars
  from /player.txt), `encodeProSigns()` (decoded text, so the echo comparison sees codes), `utf8ToCodes()` (protocol
  PUT customchars/practicechars). Out: `cleanUpProSigns()` (everything displayed), `codesToUtf8()` (protocol GET
  customchars/practicechars/snapshot, practice statistics keys).
- **Echo Trainer** joins the Decoder Chars modes; `decodedSymbol()` first returns, for a code that stands for several
  characters, the one the target word contains (`(`/<kn>, à/å, ä/æ, ö/ø, and é è ç ñ `)` which Standard shows as `*`).
  Plain equality comparison is unchanged; a genuine error in Standard still shows `*`.
- **Voice:** the extractor keys ä ö ü by their codes (same clips, same pack stamp — nothing to re-render).
- **Host tests** (scratch, not committed): the real `foldLatin1/utf8ToCodes/codesToUtf8/encodeProSigns/cleanUpProSigns/
  utf8umlaut/cleanUpText` and `generateCWword` with the real `pool[]` extracted from m32_v6.ino and compiled against a
  String shim — 15 file words, 8 decoded symbols, 17 generated codes, all as intended.
- **Found while here, fixed on master** (50ad66b, 9.1): `[ka]` `[p]` `[t]` in player files were sent as plain letters
  (brackets swapped only in the output of `utf8umlaut()`), and `GET practicechars` wrote to raw `Serial` (rule 9).
- **Docs:** manuals EN+DE (file encoding, Custom Chars, Practice Set, Decoder Chars row), changelog + what's-new,
  protocol description (character-set encoding; Practice Set commands, previously undocumented), Config Tool help.
- **Owed (bench):** a player.txt with "café für smörgåsbord niño (test) [kn] [p]" — sent codes, display (both Output
  Case settings, Font Size Small), Echo Trainer on it with Decoder Chars Standard and Sv/Fi; Custom Chars from that
  file; Practice Set picker per set; a11y Pocket: picker speaks ä ö ü ch; GET/PUT practicechars with é over USB and BLE.
