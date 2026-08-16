# Handoff: device-aware manual links in the installer and the config tool

**Goal.** A Morserino owner should never have to work out which user manual is
theirs. Both browser tools already ask the device what it is — the installer
reads the chip, the configuration tool talks the M32 serial protocol — so both
can simply offer the right manual.

**Status.** Nothing of this is built yet. The manuals it links to *are* built
and will be release assets from the next release onwards. This document is the
brief; read §6 before writing code, because one of the four cases cannot be
detected today and the answer to that shapes the work.

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

## 3. What to build

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

**Recommendation: A**, folded into whatever else goes into protocol 1.4, with
**C** as the interim so the feature can ship now. Ask Willi before choosing —
this is a product decision, not a coding one.

### 5.2 Boards that are neither classic nor Pocket

`platformio.ini` defines environments whose `HW_NAME` is `M32 WifiKit V3`,
`M32 WifiLora V3`, `M32 DevKit S3`, `M32 T190`, `minipcb_lora`. They are not
published installer targets, and no manual variant describes them. Both tools
should fall back to the manual index rather than pick one. Do not guess from
the chip family: a DevKit S3 is an ESP32-S3 and is not a Pocket.

---

## 6. How to check it works

1. `cd "Documentation/User Manual/Version 9.x" && ./build.sh all pdf all &&
   ./build.sh all epub all` — 8 PDFs and 8 EPUBs, no warnings.
2. Confirm at least one permalink resolves **after** the first release that
   runs the updated workflow. Before that they 404, and a link that has only
   ever been tested against a 404 has not been tested.
3. Installer: with a Pocket connected, install each edition in turn and confirm
   the post-install link matches what was installed. With a classic connected,
   confirm it offers the Classic manual.
4. Config tool: connect a classic and a Pocket, confirm the right link appears
   on each; unplug and confirm the link disappears with the rest of the device
   info (`showDeviceInfo` is cleared on disconnect, ~line 1009).
5. Screen reader: with VoiceOver or NVDA, confirm the link is announced with
   its full text, and that EPUB comes first for the accessibility edition.

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
