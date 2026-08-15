# Manual variant tagging — source survey (session 1, step 1)

Status: **survey complete, no source file edited yet.**
Scope of this document: what the manual sources actually look like, what that
implies for the tagging pass, and the decisions that need a human answer before
tags go in. Deliverables of the session itself (the five inventories) land next
to this file.

---

## 1. Source tree

Everything lives in **`Documentation/User Manual/Version 9.x/`**:

| File | Role |
|---|---|
| `manual_en.md` | English source, 3 950 lines, ~39 200 words |
| `manual_de.md` | German source, 4 431 lines, ~34 300 words |
| `build.sh` | pandoc → HTML → weasyprint → PDF, per language |
| `style.css` | print stylesheet (shared by both languages) |
| `title.html` / `title_de.html` | title page, injected with `--include-before-body` |
| `normalize_ids.lua` | German only: strips umlauts from heading IDs so weasyprint can resolve internal links |
| `images/` | 5 files, 4 referenced from the Markdown, 1 (`M32_logo.png`) used by the title pages |

Generated artefacts (`manual_*.html`, `m32UserManual_v9_*.pdf`) are committed
alongside the sources.

**The line-count difference between EN and DE is a wrapping artefact, not a
content difference.** EN has 169 lines longer than 90 characters (newer chapters
were written unwrapped); DE has 10. Any parallelism check must therefore compare
*structure* — headings, divs, tables — never line numbers or line counts.

### Build invocation (unchanged by this session)

```
pandoc manual_XX.md -o manual_XX.html --standalone --toc --toc-depth=3 \
  --number-sections --css=style.css --resource-path=".:images" \
  --metadata title=... --metadata lang=XX --wrap=none \
  --from markdown+fenced_divs --include-before-body=title_XX.html [--lua-filter=normalize_ids.lua]
weasyprint manual_XX.html m32UserManual_v9_XX.pdf
```

Two things about this matter for the tagging pass:

- **`--from markdown+fenced_divs` is already on.** The manual uses fenced divs
  today (see §3), so the tagging syntax needs no build change — which is what
  §2 of the briefing requires.
- `bracketed_spans` is enabled by default in pandoc's `markdown` reader, so
  `[text]{.pocket}` works as-is. Verified against the installed pandoc 3.10.1.

---

## 2. Hardware generations: there are three, not two

The briefing names three *variant keys* (`classic`, `pocket`, `pocket-a11y`) but
the manual distinguishes three *hardware generations*:

- **Morserino-32 1st edition**
- **Morserino-32 2nd edition**
- **M32 Pocket** (the manual writes it both as `M32Pocket` and `M32 Pocket`)

`classic` therefore has to cover the 1st *and* 2nd editions, and the manual
already distinguishes those two from each other in a handful of places
(e.g. "Phones Level Trimmer … 1st edition M32 does not have this",
"1st and 2nd edition only" in the audio-level appendix, differing charge times).

**Recommendation:** keep the three keys as briefed. Intra-classic differences
stay as prose exactly as they are today — they are few, they are already
explicit in the text, and splitting `classic` into two keys would double the
filter's matrix for very little gain. Those passages are listed in the
ambiguity inventory so the decision is recorded rather than assumed.

The `classic` key spans two H2 sections in "Connectors and Controls"
(`## Morserino-32 2nd edition`, `## Morserino-32 1st edition`); the Pocket has
its own (`## M32Pocket`). Those three sections are the cleanest tag sites in the
whole document.

---

## 3. Existing markup vocabulary

The manual already uses fenced divs, with four semantic classes styled in
`style.css`:

| Class | EN | DE |
|---|---:|---:|
| `note` | 36 | 35 |
| `important` | 10 | 10 |
| `warning` | 2 | 2 |
| `quote` | 1 | 1 |

Written in the shorthand form `::: note` … `:::`.

**Consequence for tagging:** a variant-specific note has to be written in the
attribute form, `::: {.note .pocket .pocket-a11y}`, because the shorthand takes
only one class. Verified that pandoc emits `class="note pocket pocket-a11y"` and
that the existing `div.note` CSS still matches, so such a note renders
identically today.

### Pandoc promotes a div containing a heading to `<section>`

When a fenced div's first block is a heading, the HTML writer merges the two:

```html
<section id="games" class="pocket" data-number="5.6">
<h2 data-number="5.6"><span class="header-section-number">5.6</span> Games</h2>
```

The heading's `id` moves onto the `<section>`. TOC links still resolve (they
point at the id, wherever it sits) and `--number-sections` is unaffected — both
verified. `normalize_ids.lua` operates on the AST's `Header` elements, before
this writer-level merge, so it is unaffected too. `style.css` uses only element
selectors (`h1`, `h2`, …) and explicit `div.note`-style class selectors — no
descendant or sibling selectors that a new wrapper could break.

This is benign but it *does* change the emitted HTML, so "byte-identical HTML"
is not the acceptance test. The acceptance test is a **visually identical PDF**,
per §8 of the briefing.

---

## 4. Structural parallelism EN ↔ DE: already excellent

Measured, not assumed:

| Property | EN | DE | Verdict |
|---|---:|---:|---|
| Headings, all levels | 156 | 156 | identical count, identical order, identical levels |
| H1 / H2 / H3 / H4 | 7 / 30 / 41 / 74 | 7 / 30 / 41 / 74 | match |
| Tables | 57 | 57 | match, and column counts match table for table |
| Table lines | 633 | 633 | match |
| Image references | 4 | 4 | match |
| Fenced divs | 98 | 96 | **one divergence** |

**The one divergence:** in *Morse Invaders → Playing the Game*
(`manual_en.md` §1625 / `manual_de.md` §1647) the English source has a `::: note`
that the German source does not have. Reported as a finding, not papered over.
It sits in a games section, i.e. `pocket`-only content that is excluded from the
accessibility edition anyway.

Because heading order matches exactly, a tag-sequence comparison script can align
the two files on headings and diff the tag list within each section. That is the
check §6 of the briefing asks for, and it will be reliable.

---

## 5. What the firmware says about the variants (authoritative, not guessed)

The briefing warns against inventing variant knowledge. For the accessibility
edition we do not have to: the firmware states the feature delta explicitly.

`Software/src/platformio.ini`, `[env:pocketwroom-accessibility]`:

```
extends = env:pocketwroom
build_flags  = ${env:pocketwroom.build_flags}  -D CONFIG_AUDIO_A11Y=1
build_unflags = -D CONFIG_CW_GAME=1
```

and `MorseMenu.cpp` / `morsedefs.h`:

```c
#ifndef CONFIG_AUDIO_A11Y     // both entries are absent from that build
    "Upload File",
    "Update Firmw",
#endif
```

So the accessibility edition **is the Pocket build minus exactly three things**:

1. all seven games (`CONFIG_CW_GAME` unflagged);
2. the WiFi menu entry **Upload File**;
3. the WiFi menu entry **Update Firmw**.

Every other `CONFIG_AUDIO_A11Y` block in the firmware is *additive* (spoken
announcements), not a feature removal. That gives a firm rule for this pass:

> Pocket content carries `{.pocket .pocket-a11y}` by default. Only the games
> chapter and those two WiFi functions carry `{.pocket}` alone.

The manual's own Appendix 6 note already says the same thing in prose, so the
tags will agree with the text rather than contradict it.

---

## 6. Where the variant-specific content actually is

154 lines in `manual_en.md` mention a variant keyword. Grouped by what the
tagging pass will do with them:

### 6.1 Clean block-level tag sites (the bulk of the work)

| Section | Tag | Size |
|---|---|---|
| `## M32Pocket` (connectors table) | `.pocket .pocket-a11y` | ~15 lines |
| `## Morserino-32 2nd edition` | `.classic` | ~20 lines |
| `## Morserino-32 1st edition` | `.classic` | ~20 lines |
| `## Games` and all seven game sections | `.pocket` | ~690 lines EN |
| `### Practice Stats` | `.pocket .pocket-a11y` | ~55 lines |
| `### CN3 Connector: touch or mechanical paddle` | `.pocket .pocket-a11y` | ~25 lines |
| `### Calibration of Battery Measurement` | `.classic` (states it is not available on the Pocket) | ~35 lines |
| `### Flipping the display screen for left-hand use` | needs a decision — says "probably only relevant for the M32Pocket" | ~14 lines |
| Parts of `## Appendix 3: Adjusting Audio Level` | `.classic` for the two blocks marked "1st and 2nd edition only" | ~40 lines |

The games chapter alone is ~17 % of the English manual, and it is the single
largest tag in the document.

### 6.2 Inline tag sites

Roughly 30 sentences and clauses of the "on the M32Pocket … / with the M32 1st
or 2nd edition …" shape, scattered through *Powering On and Off*, *Using the
ENCODER Knob and FN Button*, *The Display*, the Quick Guide and the update
appendices. These get bracketed spans, or — where a whole paragraph is
variant-specific — a div, per the briefing's "prefer the largest coherent unit".

### 6.3 Table rows — a case the briefing's vocabulary does not cover

This is the one thing encountered that the briefing did not anticipate.

Several variant-specific items are **single rows inside a shared table**, not
blocks and not inline runs:

- `### General Preferences`: *Tone Softness*, *Headphone Output*, *Theme*,
  *Invader Orient.* — each marked "(Only for M32 Pocket)" in its own cell.
- `### Preferences regarding Player Identity and Scores`: *Practice Stats* —
  "**M32 Pocket only.**"
- `### Preferences regarding Transmitting, Decoding and QSO Bot`: four rows
  whose subject is LoRa.
- `## Morserino-32 2nd edition`: *Phones Level Trimmer* — "1st edition M32 does
  not have this."

A fenced div cannot wrap a table row, and a bracketed span cannot either — a
span can only sit *inside* a cell. Three ways out, none of which this session
implements:

1. **Leave them as prose.** The rows stay in every variant and keep their
   "(Only for M32 Pocket)" marker. Zero risk, no filter work — and arguably
   correct, since a reader benefits from knowing a setting exists on the other
   model.
2. **Row marker convention.** Put an empty tagging span at the start of the
   row's first cell, e.g. `| []{.pocket}Tone Softness | … |`. A Lua filter can
   walk each `Row`, look for such a span in its first cell, and drop the row.
   Feasible, mechanical, and detectable — but it is a new convention and belongs
   in session 2 with the filter that consumes it.
3. **Split the preference tables per variant.** Rejected on sight: it duplicates
   the largest tables in the manual and guarantees drift.

**Recommendation: option 1 for this session** (leave the rows untagged, they are
already self-describing in prose), and decide between 1 and 2 in session 2 when
the filter exists. Every such row is listed in `inventory-ambiguous.md` so
nothing is lost.

---

## 7. Notation conventions found (input to `inventory-morse.md`)

Morse written as symbols turns out to be almost absent — two sites per language,
in two different notations:

| Where | Notation | Text |
|---|---|---|
| `manual_en.md:628` / `manual_de.md:654` | dot + **underscore**, backslash-escaped | `\...\_  \...\_  \...\_  \_.\_.\_` (= `vvv<ka>`) |
| `manual_en.md:2799` / `manual_de.md:3237` | dot + **hyphen** | `-. dah-dit / .- di-dah` (the *Paddle Polarity* option values) |

A third, related convention is much more common: **prosigns in angle brackets**
(`<ka>`, `<sk>`, `<as>`, `<bk>`, `<err>`, `\<HH>`), 22 occurrences in EN. These
are not Morse symbols, but they are the other notation a screen reader will
mangle — verified that pandoc escapes them correctly to `&lt;ka&gt;`, so they
survive to HTML; how a screen reader pronounces them is a session-2 question.

`dit` / `dah` also appear spelled out in prose (~22 lines), which is the form the
a11y build will want to emit for the symbol sites.

---

## 8. Decisions taken before tagging started

Answered by Willi, 2026-08-15:

1. **Pending working-tree edits** — committed on `master` first (`aa3d0cf`), so
   the tagging branch contains tagging and nothing else.
2. **`classic` granularity** — one `classic` key, as recommended. 1st-vs-2nd
   edition differences stay as prose and are listed in the ambiguity inventory.
3. **LoRa on the Pocket** — **tag it `.classic`.** The `pocketwroom-lora`
   environment was a pre-production prototype, is not in production and is not
   maintained, so no shipping Pocket has LoRa. This overrode the cautious
   recommendation above, and it is the single biggest tag in the manual after
   the games chapter: `LoRa Trx`, `Configuring LoRa Band…`, `Appendix 2` and
   three glossary entries are now classic-only.
4. **Table rows** — introduce the row-marker convention now rather than
   deferring it (see §6.3, option 2).

## 8a. Two things the briefing did not anticipate

### A div may wrap at most one section — otherwise the TOC loses it, silently

Found by the build-identity check while tagging *Uploading a Text File* and
*Updating the Firmware through WiFi* in one div. Pandoc promotes a fenced div
to `<section>` **only when it holds exactly one section**. With two or more
sibling headings inside, the div stays a plain `<div>`, the headings never
become sections, and `--toc` skips them — with no warning, and with section
numbering still correct, so nothing looks wrong until you read the contents
page. Minimal reproduction:

```markdown
# A
## B
::: {.x}
### C
### D
:::
### E
```
→ the TOC lists A, B and E. C and D are gone.

`check_build_identity.py` now verifies that every H1/H2/H3 reaches the TOC, so
this cannot happen again unnoticed.

### The row-marker convention, as adopted

An **empty span carrying the variant classes, at the very start of the row's
first cell**:

```markdown
| []{.pocket .pocket-a11y}Tone Softness | … | … |
| []{.classic}RSSI | … | … |
```

Pandoc keeps it as `Span ("",["pocket","pocket-a11y"],[]) []` in the AST and
emits `<span class="pocket pocket-a11y"></span>` — zero width, zero visual
footprint, verified against the reference PDF. A session-2 Lua filter walks each
`Row`, looks for such a span in the first cell, and drops the whole row.

The position is what makes it a row marker: an empty span at the start of the
first cell. A span anywhere else in a cell stays an ordinary inline tag. That
keeps the class vocabulary exactly as the briefing defines it.

---

## 9. Proposed deliverable location

`devdocs/manual-variants/` — this file plus the five inventories, per CLAUDE.md
§7 (developer documentation lives under `devdocs/`, grouped in a subdirectory).
