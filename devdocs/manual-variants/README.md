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

**Session 1** tagged the sources and produced the inventories, changing nothing
else. **Session 2** took the open decisions, applied the prose edits they
implied, and added the filter — so all three variants now build:

```bash
cd "Documentation/User Manual/Version 9.x"
./build.sh en pdf pocket       # one variant
./build.sh all pdf all         # 8 PDFs: 2 languages x combined + 3 variants
```

| | combined | classic | pocket | pocket-a11y |
|---|---:|---:|---:|---:|
| English | 114 pages | 86 | 101 | 75 |
| German | 120 pages | 89 | 105 | 78 |

The combined manual is still what the release ships, and still builds exactly as
before — `variant.lua` does nothing without a `variant` metadata value.

## What is here

| File | What it is |
|---|---|
| [`SURVEY.md`](SURVEY.md) | What the sources look like, the decisions taken, and the two traps the briefing did not anticipate. **Read this first.** |
| [`inventory-ambiguous.md`](inventory-ambiguous.md) | **Resolved.** The decisions taken, what was fixed, and the short list of things deliberately left alone. |
| [`inventory-menu-terms.md`](inventory-menu-terms.md) | On-device labels used in the manual, with the firmware gating that says whether they differ per variant. Generated. |
| [`inventory-images.md`](inventory-images.md) | Image references and their variant classification. Generated. |
| [`inventory-tables.md`](inventory-tables.md) | All 57 tables, with what session 2 needs to linearise them. Generated. |
| [`inventory-morse.md`](inventory-morse.md) | Morse written as symbols. Generated. |

### Tools

```bash
python3 check_parallelism.py            # EN and DE must stay structurally identical
python3 check_build_identity.py en      # combined PDF must not move
python3 check_tags_only.py <ref>        # strip the tags back out -> must equal <ref>
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

111 tags per language, identical in both:

| | `{.classic}` | `{.pocket}` | `{.classic .pocket}` | `{.classic .pocket-a11y}` | `{.pocket .pocket-a11y}` | total |
|---|---:|---:|---:|---:|---:|---:|
| fenced divs | 15 | 1 | 3 | – | 10 | **29** |
| bracketed spans | 41 | 8 | – | 1 | 24 | **74** |
| marked table rows | 3 | 1 | – | – | 4 | **8** |
| **total** | **59** | **10** | **3** | **1** | **38** | **111** |

`{.pocket}` alone means "Pocket but not the accessibility edition" — the games
and the two browser-driven WiFi functions. `{.classic .pocket}` means
"everything except the accessibility edition", and `{.classic .pocket-a11y}`
means "the two variants without games".

The largest single tags are the games chapter (`{.pocket}`, ~17 % of the
English manual) and the LoRa material (`{.classic}`).

**Verified:**

- **English and German are parallel** — same tags, in the same sections, in the
  same order (`check_parallelism.py`).
- **The combined manual has not moved** (`check_build_identity.py`), and builds
  without the filter doing anything.
- **Every variant builds with no warnings** — no dangling internal links and no
  cross-reference whose number no longer matches.
- **The tables of contents are complete** in all eight builds.

**One content divergence stands**, reported rather than papered over: the
*Playing the Game* section of Morse Invaders has a `::: note` in English that
the German text does not have. It is inside `{.pocket}` content, so it never
reaches the accessibility build.

## What is still to do

From the original briefing, not yet done:

- the `.morse` span, with `aria-hidden` symbols plus visually-hidden dit/dah
  text (only two sites per language — see `inventory-morse.md`);
- menu-term placeholders resolved from per-variant terminology files
  (`inventory-menu-terms.md` lists the 18 terms that need one, and the one
  preference whose *values* differ);
- linearising the two Koch lesson grids for the accessibility build
  (`inventory-tables.md`);
- an HTML output template: semantic headings, landmarks, scoped `<th>`;
- per-variant image paths with a fallback to common, and **alt text for all
  four images** — today they have none, so a screen reader announces nothing.

Found along the way and still open:

- **what the release ships** — see `inventory-ambiguous.md`;
- **device-aware links** from the firmware updater and the configuration tool,
  so a reader never has to know which manual is theirs;
- **cross-references as real anchors** rather than plain text, which would let
  the filter catch a reference into a section a variant removes. Extending
  `normalize_ids.lua` to rewrite link targets is a prerequisite, or German
  links will break on umlauts.
