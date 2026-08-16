# Inventory: passages that needed a human decision

**Status: resolved.** Session 1 tagged only what the prose already signalled and
listed everything else here. Session 2 took the decisions with Willi and applied
them, so this file is now a record of what was decided and why, plus the short
list of things deliberately left alone.

Maintained by hand. The four other inventories are generated.

---

## Decisions taken (2026-08-16)

| Question | Decision |
|---|---|
| May prose be rewritten so tags can work? | **Yes**, with every edit listed for review. |
| What do the variant manuals replace? | Willi leans towards the variant manuals **replacing** the combined one, with the firmware updater and config tool linking to the right one — both already ask the device which Morserino it is. That link work is a separate task; nothing is shipped differently yet. |
| Startup-screen battery claim | **Corrected.** The manual said the battery reading is "not on the M32 Pocket", but `displayStartUp()` calls `displayBatteryStatus()` unconditionally and the Pocket prints the voltage too — only the drawn symbol is classic-only. |
| Appendix 4 (firmware < 2.0) | **`{.classic}`** — firmware 1.x predates the Pocket entirely. |
| "Flipping the display screen for left-hand use" | **Left untagged** — Flip Screen exists on the classic too. |
| CP210x paragraph | **Split** — the generic instruction stays, the device name is per variant. |
| Deep-discharge block | **Advice kept for everyone**; only the "it has no prevention of deep discharge" claim is classic-only. |

---

## What was fixed

### Confirmed defects — the manual stated OLED facts as universal

**Visible lines.** The display was documented as three scrolling lines out of a
15-line buffer. `MorseOutput.h` gives the Pocket four out of 18:

```c
#ifndef CONFIG_TFT
  #define NoOfLines 15
  #define NoOfVisibleLines 3
#else
  #define NoOfLines 18
  #define NoOfVisibleLines 4
#endif
```

Corrected in all three places, in both languages.

**Startup screen.** See the decision table above. The Pocket also gained a
sentence about its logo splash, which the manual never mentioned.

### Sentences naming both variants in one grammatical unit

All split so each half carries its own tag: battery type, FN button colour,
charging current, deep-sleep consumption, the CP210x port name, the two
`update_m32` examples, the two firmware binaries, the hardware-config option
list, the *Practice Stats* ordinal, and the **FN Button** glossary entry.

### Prose that a tag alone would have broken

Two intros were rewritten rather than tagged:

- **Transceiver chapter** — the list was "the first one / the next one / the
  last one", so removing the LoRa bullet would have opened the Pocket manual's
  list with "The next one". The items are now named after the modes they
  describe (**LoRa Trx**, **WiFi Trx**, **iCW/Ext Trx**), which reads better in
  every variant and makes the bullet removable.
- **WiFi Functions** — "you can use the WiFi feature … for two functions:"
  followed by exactly the two functions the accessibility edition does not have.
  The promise is now generic and the two-function passage is `{.classic .pocket}`.

### LoRa and games outside their tagged chapters

The Pocket manuals went from 14 LoRa mentions to one — the preface, telling a
Pocket owner their device has no LoRa, which is worth keeping. The classic
manual no longer names games it cannot run.

`Generator Tx` was the interesting one: it is not just prose, the option values
genuinely differ (`Tx OFF / LoRa Tx ON / WiFi Tx ON` on classic,
`WiFi Tx OFF / WiFi Tx ON` on the Pocket). Both are now tagged.

### Cross-references — the problem session 1 did not see

Filtering renumbers the document, and all 43 English cross-references were
hand-typed section numbers. The filter's own check found 3 wrong in classic,
6 in pocket, 8 in pocket-a11y — and every one of the rest would drift on the
next edit anyway.

All of them (43 EN, 42 DE) now name the section without the number: *"see the
section **CW Keyer**"*. All variants build with no cross-reference warnings.
This also fixes the pre-existing drift problem, where inserting a section
silently invalidated the numbers elsewhere.

Appendix references were never affected: the number is part of the heading text
("Appendix 3: Adjusting Audio Level"), so it does not shift.

---

## Deliberately left alone

| What | Why |
|---|---|
| The preface's *What is new* bullet marked "M32 Pocket only:" | That block is generated from `Software/README.md` by `sync_whatsnew.py`; a hand-added tag would be overwritten on the next regeneration. It is self-describing, so it costs nothing. Teaching the generator to tag would be the fix if this ever matters. |
| **Reset Scores** row | The preference exists in every build (`extraItems[]` is not gated), so the row stays; only the list of game names is `{.pocket}`. That it does nothing useful on a classic is a firmware question, not a manual one. |
| **Phones Level Trimmer** | "1st edition M32 does not have this" — an intra-`classic` difference, which the one-`classic`-key decision keeps as prose. |
| Glossary **M32Pocket**, **MOPP**, **QSK** | M32Pocket: a classic owner may still want the term defined. MOPP and QSK describe the WiFi/EspNow path too, which the Pocket has. |
| "Flipping the display screen for left-hand use" | Per the decision above. |
| Empty alt text on all four images | New content, not filtering. Still owed — see `inventory-images.md`. |

---

## The two EN/DE content differences — resolved

Both were real, and both turned out to be more than translation slips.

**The *Playing the Game* note** (Morse Invaders) existed only in English. It
explains that the game runs in portrait by default and that *Invader Orient.*
switches it to landscape, honouring the left-handed setting. That is
information a German reader needs, so it is translated in rather than dropped.

**The hardware-config option list.** The German list named **Reset Prefs.**,
the English list omitted the option entirely — and the firmware calls it
neither. `MorsePreferences.cpp` labels slot 3 **Reset Defaults**:

```c
case 1:   str = "Calibr. Batt.";
case 2:   str = "Flip Screen";
case 3:   str = "Reset Defaults";
#ifndef LORA_DISABLED
case 4:   str = "LoRa Config.";
#endif
```

So both manuals were wrong, in different ways. Both lists now name **Reset
Defaults**, in firmware slot order, and the section that documents it says
which menu item it is.

Checked while there: the Pocket really cannot reach battery calibration —
`adjustKeyerPreference` skips slot 1 under `#ifdef CONFIG_MCP73871` — so the
`{.classic}` tags on the calibration section and on that list entry are right.

`check_parallelism.py` now exits clean.

## Still open for a later session

- **What the release ships.** See the decision table. Today the release still
  builds and uploads the combined manual only.
- **Device-aware links** from the firmware updater and the configuration tool,
  so a user never has to know which manual is theirs.
- **`status_line.png` is an OLED screenshot** shown to Pocket readers with a
  note apologising for it. A Pocket screenshot plus a per-variant image path
  would let the note go away.
- **Cross-references as real links.** They are now number-free, which makes them
  correct, but they are still plain text. Turning them into anchors would let
  the filter's dangling-link check catch a reference to a section that a variant
  removes. Note that `normalize_ids.lua` rewrites heading identifiers but not
  link targets, so it would have to be extended first or German links would
  break.
