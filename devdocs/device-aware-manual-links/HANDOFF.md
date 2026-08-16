# Handoff: device-aware manual links in the installer and the config tool

**Goal.** A Morserino owner should never have to work out which user manual is
theirs. Both browser tools already ask the device what it is — the installer
reads the chip, the configuration tool talks the M32 serial protocol — so both
can simply offer the right manual.

**Status (2026-08-16).** Built and **merged to master** (`83494cb`, branch
`device-aware-manual-links`). Both tools offer the right manual, the firmware
now says which edition it runs, and the §5.1 decision was taken: **option A**.
What is *not* done is the only part that cannot be done yet — the links point at
release assets that do not exist until the first V9 release runs the updated
workflow, so **neither tool may be published to morserino.info before that
release** (Willi's decision; see §6 and §8). What was built, and what still has
to be checked on hardware, is in §5.1 and §6.

The firmware half has to be **in** the V9 release for any of this to work: if
`edition` does not ship, every V9 Pocket takes the config tool's "does not
report its edition" fallback permanently.

---

## 1. What already exists

Since 2026-08-16 the user manual is built once per hardware variant from a
single tagged source per language (see
[`devdocs/manual-variants/`](../manual-variants/)). The release publishes twelve
assets under permanent, version-less URLs:

```
https://github.com/oe1wkl/Morserino-32/releases/latest/download/<name>

  Morserino-32_User_Manual_Classic_EN.pdf     …_Classic_EN.epub
  Morserino-32_User_Manual_Classic_DE.pdf     …_Classic_DE.epub
  Morserino-32_User_Manual_Pocket_EN.pdf      …_Pocket_EN.epub
  Morserino-32_User_Manual_Pocket_DE.pdf      …_Pocket_DE.epub
  Morserino-32_User_Manual_Pocket_Accessible_EN.pdf   …_Accessible_EN.epub
  Morserino-32_User_Manual_Pocket_Accessible_DE.pdf   …_Accessible_DE.epub
```

`releases/latest/` resolves to the newest **non-prerelease** release, so a beta
never hijacks these. The version, the month and the edition are printed on each
manual's title page, which is why the filenames carry no version.

Three variants, and they map one-to-one onto the installer's existing target
ids:

| Manual | installer target `id` | who it is for |
|---|---|---|
| `Classic` | `m32` | Morserino-32 1st and 2nd edition (OLED) |
| `Pocket` | `m32p` | Morserino-32 Pocket (TFT) |
| `Pocket_Accessible` | `m32p-a11y` | Pocket accessibility edition |

**Important:** these URLs only start resolving after the first release that
runs the updated workflow. Until then they 404. Do not ship a link that has
never been tested against a real release — check one by hand first.

*Checked 2026-08-16.* `latest` is still V8.2 (July), whose manual assets are
still called `m32UserManual_v8_{en,de}.pdf`. The permalink *mechanism* works —
that old name returns 200 through `releases/latest/download/` — but every new
name returns 404 and will until the first V9 release. Hence the publishing gate
in §8. The names the two tools build were diffed against the staging loop in
`.github/workflows/release.yml` (§7 of that file): all twelve match.

---

## 2. The two tools, and what each of them knows

### `Software/Utilities/m32_installer.html` (~1100 lines, self-contained)

Published to `https://www.morserino.info/install.html`. It reads its target
registry from `firmware/targets.json` at runtime (`REGISTRY_URL`, ~line 488),
so **the registry is data, not code** — adding a field there does not require
touching the installer's logic, but it does require republishing the file.

What it knows, and when:

| Point in the flow | What is known |
|---|---|
| after Connect | `device.chipFamily` (`ESP32` / `ESP32-S3`), `device.flashMB`, and often `device.installed` — the firmware version already on the device, read over the protocol |
| step 2, "Choose" | `target` — the entry the user is about to install, set by `selectTarget()` (~line 787). **This is exact.** |
| step 3, after a successful install | the same `target`, now definitely what is on the device |

The chip alone distinguishes classic (`ESP32`) from Pocket (`ESP32-S3`); it
does **not** distinguish the Pocket's two editions. `target` does.

### `Software/Utilities/m32_config_tool.html` (~2500 lines)

Published at the site root. It connects over the M32 serial protocol and stores
the reply to `device` in the global `deviceInfo` (~line 829, populated ~line
978). `showDeviceInfo()` (~line 1024) already renders a small card grid —
Hardware, Firmware, Build, Protocol — which is the natural place for a manual
link.

`deviceInfo.hardware` is the firmware's `HW_NAME`, or a board-version-derived
string for the classic:

- `"M32 1st edition"`, `"M32 2nd edition"` → Classic
- `"M32 Pocket (Wroom)"` → Pocket
- `"M32 WifiKit V3"`, `"M32 WifiLora V3"`, `"M32 DevKit S3"`, `"M32 T190"`,
  `"original"`, `"Unknown Device"` → see §5

---

## 3. What to build — and what was built

> **As built.** The registry carries three keys per target, not one:
> `manual` (the slug), `manualName` (the device name as it appears in the link
> text — written to be *read aloud*, which is why the classic entry says
> "Morserino-32, 1st and 2nd edition" rather than repeating the registry's
> "Morserino-32 (1st / 2nd edition)"), and `accessible` (this edition is built
> for blind operators → EPUB first). All three are optional: no `manual`, no
> link. The Choose-screen placement (item 3 below) was **not** built — the brief
> calls it secondary, and the post-install placement is the one that matters.
> Each device gets four links: EN and DE, PDF and EPUB, ordered per §4.

### Installer

1. **Add a `manual` key to each target in `Software/Utilities/targets.json`.**
   Keep it a slug, not a URL, so the base can move without editing three
   entries:

   ```json
   { "id": "m32p-a11y", "hardware": "M32 Pocket", "label": "Accessibility Edition",
     "manual": "Pocket_Accessible", … }
   ```

   Put the base URL in the installer as a constant next to `FIRMWARE_ROOT`.
   A target without a `manual` key must degrade to showing no link, never to a
   broken one — the registry is fetched at runtime and may be older than the
   page.

2. **Offer the manual on the "Installing" screen once the install succeeds**
   (`flash-done` / `flash-again-row`, ~line 428). That is the moment the tool
   knows with certainty what is on the device, and the moment the user has a
   reason to want it.

3. Optionally also on the Choose screen, next to the detected hardware.
   Secondary — the post-install placement is the one that matters.

4. **Language.** Offer EN and DE. The installer UI is English-only today; do
   not build a language switcher for this. Two links, labelled, is enough.

### Config tool

1. **Map `deviceInfo.hardware` to a manual slug**, with an explicit table, not
   substring guessing. `"M32 Pocket (Wroom)"` must not be matched by a rule
   that also swallows `"M32 DevKit S3"`.

2. **Render the link inside `showDeviceInfo()`** as one more card, or as a line
   beneath the grid. It already runs on every connect.

3. **Unknown hardware string → link to the manual index page**
   (`https://github.com/oe1wkl/Morserino-32/tree/master/Documentation/User%20Manual`),
   not to a guess. See §5.

---

## 4. Accessibility — this one matters more than the rest

The whole point of the accessibility edition is that its users cannot skim a
page to find the right download. So in both tools:

- give the link **real link text** ("User manual for the M32 Pocket,
  Accessibility Edition — EPUB"), never "click here" and never a bare URL;
- **put EPUB first** for the accessibility variant. A screen reader and a
  braille display handle EPUB far better than a PDF, which has no reliable
  reading order. PDF stays available, second;
- if the link appears as a result of an action (install finished), make sure it
  is announced — the installer already uses `role="status"` and
  `aria-live="polite"` on its status lines; put the link inside a region that
  is announced, or move focus to it;
- do not encode the variant only in colour, an icon, or position.

---

## 5. Cases that need a decision before coding

### 5.1 The Pocket's two editions are indistinguishable over the protocol

`MorseJSON::jsonDevice()` (`MorseJSON.cpp` ~line 64) reports exactly four
fields — `hardware`, `firmware`, `protocol`, `build` — and
`pocketwroom-accessibility` extends `pocketwroom`, so **both Pocket editions
report `HW_NAME = "M32 Pocket (Wroom)"`**. Nothing in the protocol says which
edition is running.

The installer does not care: it knows `target`. The config tool does. Options:

| | Approach | Cost |
|---|---|---|
| **A** | Add a field to the protocol's `device` reply, e.g. `"edition": "accessibility"` (or a general capability list). Clean, exact, and useful to every protocol client. | A firmware change and a protocol version bump. Protocol 1.4 is already the bucket for deferred additions (game-score commands, `GET capabilities`) — see `devdocs/protocol-audit/`. |
| **B** | Infer from the menu tree the config tool already fetches into `allMenus`: the accessibility build has no **Upload File**, no **Update Firmw** and no **Games**. | No firmware change, but it is a heuristic, and it breaks quietly the day one of those entries moves. |
| **C** | Link the plain Pocket manual and mention the accessibility edition in the link text. | Trivial; slightly wrong for exactly the users who can least afford it. |

**Decided 2026-08-16 (Willi): A** — but *without* waiting for protocol 1.4 and
without bumping the version.

`MorseJSON::jsonDevice()` now emits a fifth property:

```
{"device":{"hardware":"M32 Pocket (Wroom)","firmware":"9.0","protocol":"1.3",
           "build":"Aug 16 2026","edition":"accessibility"}}
```

`edition` is `"accessibility"` under `CONFIG_AUDIO_A11Y` and `"standard"`
otherwise — on every variant, so a client never has to reason about which
builds have it. The protocol version stays **1.3**: the property is purely
additive, the protocol description already tells clients to ignore properties
they do not recognise, and a client that wants it just tests whether it is
there. That keeps 1.4 free for the batch it was reserved for (game-score
commands, `GET capabilities`). Documented in
`Documentation/Protocol Description/M32 Protocol.md` — the `GET device`
section, "Versioning and Compatibility", and a dated revision note in the
header, following the precedent of the BLE-transport note.

**Absence is "not known", not "standard".** Every Pocket in the field today
predates the property, so the config tool's fallback path is the *common* path
for a while: it links the standard Pocket manual and adds a line saying the
firmware does not report its edition, with a link to the manual index for
anyone whose Pocket speaks its menus aloud. That is option **C**, but only
where the answer is genuinely unknown — never in place of the answer.

Option B (inferring from the menu tree) was not built, and the case against it
proved itself while this was being written: the accessibility build **lost** its
**Upload File** and **Update Firmw** entries in `94adfcd`, landing on master the
same day. That is exactly the quiet drift that would have broken the heuristic —
and it would have broken it silently, into a wrong manual rather than an error.

### 5.2 Boards that are neither classic nor Pocket

`platformio.ini` defines environments whose `HW_NAME` is `M32 WifiKit V3`,
`M32 WifiLora V3`, `M32 DevKit S3`, `M32 T190`, `minipcb_lora`. They are not
published installer targets, and no manual variant describes them. Both tools
should fall back to the manual index rather than pick one. Do not guess from
the chip family: a DevKit S3 is an ESP32-S3 and is not a Pocket.

**As built.** `MANUAL_BY_HARDWARE` in the config tool matches whole hardware
strings, never substrings, and holds seven entries:

- Classic → `M32 1st edition`, `M32 2nd edition`, `unknown M32 board`,
  `original`. The first three are derived from the board revision in
  `m32_v6.ino` (~line 1044) and **only the classic build can report them** —
  `ORIGINAL_M32` is defined in `heltec_wifi_lora_32_V2` alone — so this is
  reading the registry of `HW_NAME`s, not guessing. `original` is that
  environment's own `HW_NAME`, which older firmware sent before the
  board-revision naming existed.
- Pocket → `M32 Pocket (Wroom)`, `M32 Pocket (Wroom LoRa)`,
  `M32 Pocket (Wroom 170x240)`. The last two are prototypes that never became
  installer targets, but they run Pocket firmware, so the Pocket manual is
  still theirs.

Everything else — WifiKit V3, WifiLora V3, DevKit S3, T190, minipcb, and any
`HW_NAME` added after this was written — falls through to the manual index.

---

## 6. How to check it works

**Done (2026-08-16).**

- Firmware builds clean for `heltec_wifi_lora_32_V2`, `pocketwroom` and
  `pocketwroom-accessibility`.
- The fifth property still fits `jsonDevice()`'s `StaticJsonDocument<256>` —
  worth checking rather than assuming, because ArduinoJson 6 truncates a full
  document *silently*, and a truncated `device` reply is the one message the
  config tool refuses to connect without. Compiled against the project's own
  ArduinoJson 6.20.1 with the longest `HW_NAME` and a three-part version:
  `overflowed() == false`, 227 of 256 bytes on a 64-bit host — and that is the
  pessimistic number, since ArduinoJson's slots are half the size on the
  32-bit ESP32 (~131 bytes there).
- Both tools' link builders were run against the real `targets.json` and the
  real hardware strings, without retyping them: the installer's `manualLinks()`
  was sliced out of the shipped HTML and executed, and the config tool's
  `showDeviceInfo()` was called in a browser with six device replies (classic,
  Pocket standard, Pocket accessibility, Pocket without `edition`, DevKit S3,
  old classic firmware). All twelve generated URLs match the names
  `release.yml` stages; EPUB comes first for the accessibility edition; a
  target with no `manual` slug and an unknown `HW_NAME` both degrade as
  intended.
- Link contrast: the links inherit the finished box's dark green in the
  installer (5.6:1) and use `#155e8f` in the config tool (6.0:1 on the box).
  `var(--link)` (`#268bd2`) was **not** used — it scores 3.0:1 on the green box
  and 3.7:1 on white, both below AA for text this size. Colour is not carrying
  the meaning anywhere: every link is underlined and fully described in words.
- The manual index URL used as the fallback returns 200.

**Still open — needs hardware, or the release.**

1. Confirm at least one permalink resolves **after** the first release that
   runs the updated workflow. Before that they 404, and a link that has only
   ever been tested against a 404 has not been tested. This is the gate on
   publishing (§8).
2. Installer: with a Pocket connected, install each edition in turn and confirm
   the post-install link matches what was installed. With a classic connected,
   confirm it offers the Classic manual.
3. Config tool: connect a classic and a Pocket, confirm the right link appears
   on each; unplug and confirm the link disappears with the rest of the device
   info (`showDeviceInfo` is cleared on disconnect). With V9 firmware flashed,
   confirm the Pocket a11y build reports `"edition":"accessibility"` and that
   the "does not report which edition" line is *gone*.
4. Screen reader: with VoiceOver or NVDA, confirm the link is announced with
   its full text, and that EPUB comes first for the accessibility edition. In
   the installer the whole finished message — confirmation and links together —
   sits in one `role="status"` region and is filled in before the region is
   unhidden, so it should be announced in one go; that is the part worth
   listening to.
5. `cd "Documentation/User Manual/Version 9.x" && ./build.sh all pdf all &&
   ./build.sh all epub all` — 8 PDFs and 8 EPUBs, no warnings. Unchanged by
   this work, but it is what produces the files the links point at.

## 7. Out of scope

- Do not rework the manuals, the tagging, or `variant.lua` — that work is done
  and merged; see `devdocs/manual-variants/`.
- Do not add a language switcher to either tool.
- Do not bundle manuals into the tools or host copies elsewhere; the release
  assets are the single source.
- Do not change `release.yml`'s asset names. They are permalinks now, and the
  READMEs, and in due course morserino.info, point at them.

## 8. Publishing

Both tools live in this repository and are published to `morserino.info` by
`~/sync-to-dropbox.sh`, which copies whatever is checked out — it has no git
logic, so **commit before publishing**. `targets.json` is served from
`firmware/targets.json` and is fetched at runtime with `cache: 'no-cache'`; a
stale copy in a browser is not the usual failure, an unpublished copy is.
`scripts/release/dropbox_publish.sh` handles the firmware side during a release.

**Gate (Willi's decision, 2026-08-16): do not publish either tool until the
first V9 release has run.** The manual links are correct but the assets they
point at do not exist yet, and a user who clicks one before the release gets a
GitHub 404. Publishing after the release costs nothing and the links are right
from the first moment anyone can see them. Two files have to go out together —
`m32_installer.html` **and** `firmware/targets.json` — because the installer
reads the `manual` slugs from the registry at runtime: publishing the page
without the registry silently produces no manual links at all, which is the
designed degradation, not an error anyone will notice.
