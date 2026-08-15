# Inventory: passages that need a human decision

Maintained by hand (unlike the other four inventories, which are generated).
This is the most important output of session 1: **everything the tagging pass
deliberately did not tag, and why.** Nothing here was guessed at — where the
manual does not say a passage is variant-specific, it was left alone.

Line numbers are for the tagged sources as committed on `manual-variant-tagging`.

Nine categories, most urgent first.

---

## 1. Confirmed defects — the manual states classic-only facts as universal

Not ambiguous at all: the firmware disagrees with the manual. Fixing these means
rewriting prose, which this pass is not allowed to do, so they are reported.

### 1.1 The number of visible text lines is wrong for the Pocket

`manual_en.md:368, 369, 372, 385, 395` · `manual_de.md:392, 395, 408, 418, 419`

The manual says the display shows **three** scrolling lines out of a **15**-line
buffer. That is the OLED. `Software/src/Version 6 and newer/MorseOutput.h`:

```c
#ifndef CONFIG_TFT
  #define NoOfLines 15
  #define NoOfVisibleLines 3
#else
  #define NoOfLines 18
  #define NoOfVisibleLines 4
#endif
```

So on the Pocket it is **four** visible lines out of **18**. Stated three times
in each language: in the scroll-mode paragraph, in the display description, and
in the scroll-buffer paragraph.

**Question:** reword to name both numbers, or split into two tagged sentences?
The second is cheap once you decide the wording.

### 1.2 After filtering, the Pocket manual says nothing about its startup screen

`manual_en.md:253–256` (now `{.classic}`) · `manual_de.md:270–273`

The two sentences describing the startup screen — LoRa QRG on the top line,
battery indication at the bottom — are explicitly "not on the M32 Pocket", so
they are tagged `{.classic}`. Correct in itself, but it leaves the Pocket
manual with no description of its own startup screen at all.

**Question:** one or two Pocket sentences to fill the hole. (For the a11y
edition the firmware already announces version and battery voltage aloud at
startup — worth documenting there too.)

---

## 2. Sentences that name both variants in one grammatical unit

The tagging vocabulary cannot split these: the two halves share a comma, a
semicolon or a verb, so any tag leaves a fragment behind. Splitting them means
rewriting prose — out of scope for this pass (briefing §2).

| EN | DE | Passage | What it needs |
|---|---|---|---|
| 122 | 129–130 | "M32 1st and 2nd generation use a 3.7 V LiPo battery, the M32Pocket uses a 14500 type Li-Ion cell" | split into two sentences, then one tag each |
| 133 | 140 | "(red on the first and second edition Morserinos; integrated into the case on the M32Pocket)" | same |
| 232 | 247 | "it consumes a max of 200 mA, or 500 mA for the M32Pocket, when charging" | same |
| 247 | 263 | "less than 5% of normal operation with M32 1st or 2nd edition, around 1% with M32Pocket" | same |
| 322 | 341 | "no prevention of deep discharge (this is true for 1st and 2nd edition Morserinos; the M32Pocket also prevents deep discharge)" | see §3 below — this one is more than a split |
| 3185–3186 | 3618 | "This functions behaves differently for the M32Pocket, vs. the 1st and 2nd edition M32!" | a comparison note; probably drop it per variant rather than split |
| 3289 | 3726 | "two different binaries for each version, one for Morserinos 1st and 2nd edition, and one for the M32Pocket" | in a filtered manual only one binary is relevant |
| 3364 | 3806 | "You no longer have to know whether you own a classic Morserino-32 or an M32 Pocket" | reads oddly in a manual that already knows which one you own |
| 3318–3320 | 3754 | the two `update_m32` examples differ in **both** OS and device, so neither half is cleanly taggable | split into per-device examples |
| 2953–2954 | 3377 | the Hardware Config option list names **LoRa Config.** and **CN3: Touch / CN3: Mechan.** in one sentence | one list per variant |
| 2931 | 3357 | "The fourth (M32 Pocket only) switches Practice Stats logging on or off" — ordinal prose over a list whose length is variant-dependent | reword without the ordinal |
| 3970 | 4448 | glossary **FN Button**: "red on 1st/2nd edition, integrated into the case on M32Pocket" | split, or leave as a glossary fact |

**Recommendation:** these are all one- or two-sentence edits, and they are the
single biggest source of "wrong for my device" text in the filtered manuals.
Worth a dedicated editing pass before session 2's filter goes live.

---

## 3. The deep-discharge warning is classic advice given to everyone

`manual_en.md:314–329` · `manual_de.md:333–348`

An `::: important` telling you to always switch off via the slide switch to
prevent deep discharge, then a paragraph saying the charger has no deep-discharge
protection — *"the M32Pocket also prevents deep discharge"* — then a `::: warning`
about killing the battery.

For a Pocket owner the warning is at best over-stated: their device does have
protection, and the manual says so in the same breath. Deciding what a Pocket
reader should be told is a product judgement, not a tagging one.

**Question:** does the Pocket's protection make the warning unnecessary, or is it
still good practice? Answer determines whether this block gets `{.classic}`, gets
split, or stays as it is.

---

## 4. LoRa references left standing outside the tagged LoRa sections

Tagging LoRa `{.classic}` (your decision: `pocketwroom-lora` was a
pre-production prototype, not in production, unmaintained) removes the LoRa
chapters from both Pocket manuals. These mentions sit in **untagged** text and
will therefore still appear in the Pocket manual, pointing at material that is
no longer there:

| EN | DE | Where | Text |
|---|---|---|---|
| 590 | 619 | CW Memory Keyer | "not in the **WiFi Trx** or **LoRa Trx** modes" |
| 1262, 1265 | 1267, 1270 | Transceiver intro | "three or four transceiver modes … depending on the availability of LoRa"; first bullet is "If you have LoRa…" — see §5.1 |
| 1351 | — | WiFi Trx | "this is then very similar to using LoRa". **The German translation already omits this clause** — worth knowing that the two texts differ here in substance, not just wording. |
| 1435 | 1442 | QSO Bot | "it never transmits over LoRa, WiFi or the external transmitter" |
| 460 | 487 | The Display | the wireless-indicator bullet describes both the WiFi symbol and the LoRa symbol |
| 2714 | 3145 | Snapshots | the list of settings a snapshot does *not* store includes "the LoRa channel" |
| 2915 | 3341 | Transmitting prefs intro | "either directly through LoRa or Wifi" |
| 2921, 2922, 2923 | 3347, 3348, 3351 | preference rows | **Key ext TX**, **Generator Tx**, **Trx Channel** all describe LoRa behaviour |
| 2953, 3071 | 3377, 3504 | Appendix 1 | "**LoRa Config.**" in the hardware-menu option list, and in the numbered steps |

Note that **Generator Tx really does differ**: its option values are
`Tx OFF / LoRa Tx ON / WiFi Tx ON` on classic and `WiFi Tx OFF / WiFi Tx ON` on
the Pocket (see `inventory-menu-terms.md`). That one is a genuine terminology
entry for session 2, not just a prose fix.

**Question:** which of these get `{.classic}` (safe where the whole sentence is
about LoRa) and which need rewording (where LoRa and WiFi share a sentence)?

---

## 5. Tagging would leave broken prose — needs a rewrite, not a tag

A tag that leaves the surviving text ungrammatical is not a tag, it is a rewrite
request. Two places hit this:

### 5.1 Transceiver chapter intro — `manual_en.md:1262–1281` · `manual_de.md:1267–1286`

A three-item list: "If you have LoRa, **the first one** is…", "**The next one**
uses the Internet Protocol…", "**The last one** is…". Tagging the LoRa bullet
`{.classic}` would open the Pocket manual's list with *"The next one"*. Left
untagged; the intro needs one version per variant, or ordinal-free wording.

### 5.2 WiFi Functions intro — `manual_en.md:2289–2313` · `manual_de.md:2723–2747`

"…you can use the WiFi feature … for two functions of the device:" followed by
two bullets, both of which are exactly the functions the accessibility edition
does **not** have (Upload File, Update Firmw). The two sub-sections are tagged
`{.classic .pocket}`, but tagging the intro would leave the a11y manual with a
sentence promising two functions and an empty list. Left untagged.

---

## 6. Whole list items that are variant-specific

Fenced divs cannot wrap a list item without reindenting its continuation lines,
and reindenting is reformatting (briefing §8: a diff that shows rewrapped text
is unreviewable). Left untagged:

| EN | DE | Item |
|---|---|---|
| 38 | 41–43 | Preface, "What is new": "For the M32 Pocket only: Battery level and charging status…" |
| 40 | 44–48 | Preface, "What is new": "Also for the M32 Pocket only: We have begun to implement games…" |
| 1265–1267 | 1270–1272 | Transceiver intro, "If you have LoRa, the first one is…" (see §5.1) |

**Option for session 2:** a *whole-item span* convention — wrap the entire item
text in `[...]{.classic}` and have the filter drop list items that become empty.
That needs no reindenting and no reflow. It is a second convention beyond the
briefing's vocabulary, so it is proposed here rather than applied.

---

## 7. Proposed tags awaiting a yes

High confidence, but the text does not say it outright, so per briefing §4 they
were left untagged:

| Where | Proposed | Reasoning |
|---|---|---|
| `manual_en.md:3231` / `manual_de.md:3666` — **Appendix 4: Updating the Firmware via WiFi for Versions < 2.0** | `{.classic}` | Firmware 1.x predates the Pocket entirely; no Pocket can be on a version < 2.0. The appendix is short, so the cost of being wrong is small either way. |
| `manual_en.md:2988` / `manual_de.md:3416` — **Flipping the display screen for left-hand use** | `{.pocket .pocket-a11y}`? | The text hedges: *"This is probably only relevant for the M32Pocket!"* But **Flip Screen** is listed as a hardware-config option without a Pocket-only marker, so it appears to exist on both. Does it work on the classic OLED, and is it useful there? |
| `manual_en.md:3268` / `manual_de.md:3707` — "Connect your Morserino with a USB cable … should indicate something like: Silicon Labs CP210x … (COM3)" | split | The paragraph mixes a generic instruction ("connect your Morserino") with a classic-only detail (the CP210x device name). The two driver paragraphs before it are already `{.classic}`. |

---

## 8. Table rows deliberately left unmarked

The row-marker convention is in place and used where a row simply does not exist
on a variant. These rows were **not** marked, on purpose:

| Row | EN | Why not marked |
|---|---|---|
| **Key ext TX**, **Generator Tx**, **Trx Channel**, **Decoded on I/O** | 2921–2925 | The *preference* exists on both; only its description mentions LoRa (or, for Decoded on I/O, says the Pocket ignores it). Dropping the row would remove a setting the reader has. |
| **Reset Scores** | 2937 | Present in every build (`extraItems[]` is not gated) — but on the classic there are no games for it to reset, and the row's text lists Pocket-only game names. Odd on the classic; a firmware question as much as a manual one. |
| **Phones Level Trimmer** (2nd-edition connector table) | 74 | "1st edition M32 does not have this" — an intra-`classic` difference, which the one-key decision keeps as prose. |
| glossary **M32Pocket**, **MOPP**, **QSK**, **FN Button** | 3976, 3979, 3982, 3970 | M32Pocket: a classic owner may still want the term defined. MOPP and QSK describe the WiFi/EspNow path too, which the Pocket has. FN Button is a conjoined sentence (§2). |

---

## 9. Loose ends worth recording

- **The Preface still says "What is new in Version 8?"** (`manual_en.md:27`,
  `manual_de.md:28`) in a Version 9 manual. Not a tagging matter, but the two
  Pocket-only bullets under it (§6) become trivial to tag once the section is
  rewritten for V9.
- **File Player on the accessibility edition.** The Appendix 6 note says the
  a11y edition uses the SPIFFS space that holds the File Player's text for its
  voice clips, and the edition also drops **Upload File** — so there is no way
  to get text onto the device. Yet **File Player** is still in the menu
  (`menuText[]` is not gated for it). Is File Player usable at all on the a11y
  edition, and if not, should the manual say so?
- **Every image reference has empty alt text** (all four, both languages). For
  the accessibility build a screen reader announces nothing at all. New content,
  so out of scope here — see `inventory-images.md`.
- **One genuine EN/DE content divergence**, reported by `check_parallelism.py`
  and left standing: the *Playing the Game* section of Morse Invaders has a
  `::: note` in English that the German text does not have
  (`manual_en.md:1650`, in the *Playing the Game* subsection). Inside `{.pocket}` content, so it never reaches the
  a11y build.
- **`status_line.png` is an OLED screenshot shown to every reader**, with a note
  apologising for it. See `inventory-images.md`.
