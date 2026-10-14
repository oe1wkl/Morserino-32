# Preference review — notes and open questions (TODO B16 / B17)

Working document for Willi's review of the preference order (B16) and of which preferences each mode offers
(B17). The facts are generated into `PREFERENCE_MATRIX.md` by `make_matrix.py` (re-run after any change);
this file holds the observations and, once made, the decisions. Prepared 2026-10-14 on `m32kip`; nothing in
the firmware or the manual has been changed yet.

**Mechanics when you decide to move something** (CLAUDE.md §3 rule 10): the `prefPos` enum, `pliste[]` and
`prefName[]` are positional and move together — *but the display order does not need that*: what the operator
scrolls through is the order of the option lists (`allOptions[]` and the per-mode `xxxOptions[]` in
`MorsePreferences.cpp`). Reordering those lists touches no storage (NVS keys and snapshots go by name). It does
change the order of the protocol's config list and the a11y tables, so re-run the voice extractor afterwards
(expected: no new clips).

## B17 — offered where it does nothing, or missing where it acts

Evidence is section 3 of the matrix (where each preference is read) plus the conditions at those reads.

1. **Headphone Output is missing in CW Generator** (Random / Abbrevs / Words / Calls / Mixed / Practice Set),
   and only there: every other mode, including the generator's own File Player, offers it.
   `generatorOptions[]` lacks the `LINEOUT` macro that all the other lists carry. Looks like an oversight.
2. **Key ext TX is not offered in iCW/Ext Trx (CW Trx)**, although it acts there: in `keyOut()` its values
   *CW Keyer only* ("also CW Trx", says the code) and *Keyer & Gen & Trx* key the transmitter in `morseTrx`.
   An operator in CW Trx cannot reach the one setting that decides whether the rig is keyed.
3. **Tone Shift is offered in iCW/Ext Trx and in the QSO Bot, where it has no effect.** It is read for the
   Echo Trainer, LoRa Trx, WiFi Trx, the generator's `[t]` file marker and two games — not for `morseTrx` or
   `morseQsoBot`.
4. **Tone Shift acts in CW Generator: File Player but is not offered there.** The file marker `[t]` (next
   station) toggles the pitch by the Tone Shift setting (`pitch()` in `m32_v6.ino`). Side note: with Tone
   Shift at *No Tone Shift* the `[t]` marker still shifts — down a semitone (`value == 1 ? up : down`).
5. **Bandwidth is offered in the QSO Bot, where no audio is decoded** (the bot decodes your key; `MorseQsoBot.cpp`
   never touches the Goertzel decoder). It does act in iCW/Ext Trx and the CW Decoder.
6. **Bluetooth Use and BLT \<AR\> are offered in every mode.** To check: whether changing *Bluetooth Use* from
   inside, say, the CW Generator takes effect at once, on the next mode start, or only at the menu (BLE Serial
   is started from the top-menu loop; the keyboard from CW Keyer). If it is "next time", offering it in every
   mode is at least surprising.
7. Fine as they are, listed so they are not re-examined: the general block (Encoder Click … Serial Output)
   everywhere; Decoder Chars absent from the QSO Bot (the bot uses Standard on purpose) and from the a11y
   build; Echo-only settings absent from the generators; Remote Rig's list exists only for tools (the mode has
   no preferences gesture).

## B16 — order

1. **Mode lists and the All view disagree about where the Bluetooth pair goes.** In every mode list
   *Bluetooth Use* and *BLT \<AR\>* come right after Theme / Font Size, before Serial Output (the `BLUE`
   macro sits there); in the All view they come after Latency, at the end of the keyer group (where the manual
   has them too).
2. **Three places where the manual's tables and the All view differ:**
   - *Practice Set*: the manual gives it its own section between Koch Sequence and CW Generation; the device
     has it inside the generation group, after *Length Rnd Gr*.
   - *Decoder Chars*: manual — Bandwidth, Decoder Chars, Decoded on IO; device — Bandwidth, Decoded on IO,
     Decoder Chars.
   - *QSO Bot*: manual — Contest Type, QSO Difficulty; device — QSO Difficulty, Contest Type.
3. **Question for the review:** inside a mode the first twelve entries are always the general block (Encoder
   Click, Tone Pitch, Tone Softness, Time-out, Quick Start, Output Case, Headphone Output, Theme, Font Size,
   Bluetooth ×2, Serial Output), so the settings that belong to the mode start at 13–15. Do you want the
   mode's own settings first and the general block after them? (Accessibility angle: a blind operator hears
   every entry on the way down.)
4. The manual's *List of All preferences* chapter and the device's All view otherwise agree in order, section
   by section.
