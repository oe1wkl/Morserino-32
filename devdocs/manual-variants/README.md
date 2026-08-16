# Manual variant tagging

For V9 the Morserino-32 user manual builds into **three variants from one
tagged source** per language, rather than being forked:

| Variant key | Target |
|---|---|
| `classic` | Classic Morserino-32, 1st and 2nd edition (OLED) |
| `pocket` | Morserino-32 Pocket (TFT) |
| `pocket-a11y` | Pocket accessibility edition, for blind operators |

`pocket-a11y` is a filtered variant of the Pocket manual, not a separate
document: it keeps every visual reference ("the display shows…") and differs
only where the accessibility firmware genuinely differs.

**Session 1 (this directory) tagged the sources and produced the inventories.
No filter, no template and no build change were made** — with no filter in
place pandoc renders fenced divs and bracketed spans transparently, so both
PDFs still build byte-for-byte the same page count with identical text on every
page. That is the proof the pass broke nothing.

## What is here

| File | What it is |
|---|---|
| [`SURVEY.md`](SURVEY.md) | What the sources look like, the decisions taken, and the two traps the briefing did not anticipate. **Read this first.** |
| [`inventory-ambiguous.md`](inventory-ambiguous.md) | **The important one.** Every passage deliberately left untagged, and the question that has to be answered before it can be. |
| [`inventory-menu-terms.md`](inventory-menu-terms.md) | On-device labels used in the manual, with the firmware gating that says whether they differ per variant. Generated. |
| [`inventory-images.md`](inventory-images.md) | Image references and their variant classification. Generated. |
| [`inventory-tables.md`](inventory-tables.md) | All 57 tables, with what session 2 needs to linearise them. Generated. |
| [`inventory-morse.md`](inventory-morse.md) | Morse written as symbols. Generated. |

### Tools

```bash
python3 check_tags_only.py [ref]        # strip the tags back out -> must equal the pre-tagging source
python3 check_build_identity.py en      # the acceptance test: PDF must not move
python3 check_build_identity.py de
python3 check_parallelism.py            # EN and DE must stay structurally identical
python3 make_inventories.py             # regenerate the four generated inventories
python3 tagmap.py                       # what is tagged, per language
python3 firmware_terms.py               # the on-device vocabulary, with #ifdef gating
```

`check_parallelism.py` exits 1 while one known content divergence stands (see
below). `tagmap.py` and `firmware_terms.py` are libraries the others import;
running them directly just prints what they see.

## Tagging vocabulary

Fenced divs for blocks, bracketed spans for inline runs:

```markdown
::: {.pocket .pocket-a11y}
This section applies to both Pocket editions.
:::

The paddle is connected to [CN3]{.pocket}[the 3.5 mm jack]{.classic}.
```

- **Untagged means "all variants"** and is the overwhelming majority of the text.
- Classes are exactly `classic`, `pocket`, `pocket-a11y`.
- Multiple classes mean "include in any of these".
- `pocket-a11y` is **not** implied by `pocket` — content for both Pocket
  editions carries both classes.
- A note keeps its own class alongside: `::: {.note .pocket .pocket-a11y}`.
- **One div wraps at most one section.** A div spanning two sibling sections
  silently drops both from the table of contents — see `SURVEY.md` §8a.
- Table rows are marked with an **empty span at the start of the first cell**:
  `| []{.classic}RSSI | … |`.

## Result

37 tags per language, identical in both:

| | `{.classic}` | `{.pocket}` | `{.classic .pocket}` | `{.pocket .pocket-a11y}` | total |
|---|---:|---:|---:|---:|---:|
| fenced divs | 13 | 1 | 2 | 9 | **25** |
| bracketed spans | 2 | – | – | 2 | **4** |
| marked table rows | 3 | 1 | – | 4 | **8** |
| **total** | **18** | **2** | **2** | **15** | **37** |

`{.pocket}` alone means "Pocket but not the accessibility edition" — the games
and the two browser-driven WiFi functions. `{.classic .pocket}` means
"everything except the accessibility edition".

The largest single tags are the games chapter (`{.pocket}`, ~17 % of the
English manual) and the LoRa material (`{.classic}`).

**Verified**, four ways:

- **Nothing but tags changed** — stripping every tag back out reproduces the
  pre-tagging sources exactly, in both languages (`check_tags_only.py`).
- **The PDF did not move** — EN 114 pages, DE 118 pages, identical text on every
  page against the pre-tagging PDFs (`check_build_identity.py`).
- **The table of contents is complete** in both languages.
- **EN and DE are parallel** — same tags, same sections, same order
  (`check_parallelism.py`).

**One content divergence stands**, reported rather than papered over: the
*Playing the Game* section of Morse Invaders has a `::: note` in English that
the German text does not have (`manual_en.md:1650`).

## What session 2 does

From the briefing: the Lua filter for variant stripping; the `.morse` span with
`aria-hidden` symbols plus visually-hidden dit/dah text; menu-term placeholder
resolution from per-variant terminology files; table linearisation; an HTML
output template with semantic headings, landmarks and scoped `<th>`; per-variant
image paths with a fallback to common.

Three additions this session found, in priority order:

1. **A prose pass over `inventory-ambiguous.md` §2 first.** A dozen sentences
   name both variants in one grammatical unit and cannot be tagged without
   rewriting. They are the single biggest source of "wrong for my device" text
   the filter will leave behind, and they are all one- or two-sentence edits.
2. **The filter must drop marked table rows**, and — if the whole-item span
   convention in §6 is adopted — list items that become empty.
3. **Alt text for the four images.** Every image reference in the manual has
   empty alt text, so a screen reader announces nothing at all.
