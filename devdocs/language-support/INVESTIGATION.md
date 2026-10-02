# Decoder: ITU brackets and national characters — investigation

*2026-10-02. Prompted by two tester requests (TODO I3): decoding per ITU-R M.1677-1, and characters for
French, Spanish, Portuguese and the Nordic languages. Investigation only — nothing is implemented yet.*

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
