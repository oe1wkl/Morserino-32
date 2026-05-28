# Morserino-32 — Release Automation Design

Status: **draft for review** — no code written yet. Implementation begins only after this document is approved.

---

## 1. Goal

Replace the current manual, multi-step release process with a tag-triggered GitHub Actions workflow that:

1. Builds firmware for the two distributed hardware platforms.
2. Rebuilds the PDF user manuals from markdown.
3. Creates a GitHub Release (stable or pre-release, auto-detected from the tag).
4. Attaches all assets (firmware binaries, English/German manuals, Pocket FAQ).
5. Copies firmware into the local Dropbox folder that backs the web installer.
6. Prepends a new entry to each platform's `versions.json` manifest.
7. For **stable** releases only: commits the new binaries to the in-repo archive at `Software/binary/From Version 7 onwards/…` on the default branch (`master`), so users with scripted retrieval from the repo continue to find them there.

The developer's only manual steps after this is in place:

- Write/update the changelog section in `Software/README.md`.
- Tag the commit (`git tag V8.1 && git push --tags`).
- Watch the workflow finish.

---

## 2. Non-goals

- **No automatic changelog generation from commits.** The developer maintains release notes by hand in `Software/README.md`; the workflow extracts the matching section.
- **No automatic version bumping in source code.** The tag *is* the version.
- **No Dropbox API integration.** The runner is the Mac itself; it writes to Dropbox via the filesystem and lets the Dropbox client sync.
- **No support for the 7 experimental/prototype PlatformIO environments** (`heltec_wifi_kit_32_V3`, `heltec_wifi_lora_32_V3`, `ESP32_S3_Devkit`, `heltec-vision-master-t190`, `pocketwroom-lora`, `pocketwroom-170x240`, `minipcb_lora`). They are ignored by the release workflow.
- **No removal of the existing `pio-ci.yml`** smoke-test workflow. It continues to run per-push on `heltec_wifi_lora_32_V2` and provides early breakage detection.

---

## 3. Architecture overview

```
       developer Mac                                  GitHub
   ┌────────────────────┐    git push --tags    ┌──────────────┐
   │ git tag V8.1       │ ────────────────────▶ │  github.com  │
   │ git push --tags    │                       └──────┬───────┘
   └────────────────────┘                              │ webhook
                                                       ▼
                                          ┌─────────────────────────┐
                                          │ GitHub Actions schedules│
                                          │ release.yml on a self-  │
                                          │ hosted runner labelled  │
                                          │ "macos-morserino"       │
                                          └─────────────┬───────────┘
                                                        │
                                                        ▼
       developer Mac                       ┌─────────────────────────┐
   ┌─────────────────────────┐             │ runner (this Mac)       │
   │ Dropbox client          │ ◀───── fs ──│  1. checkout            │
   │   firmware/             │             │  2. pio run -e ...      │
   │   firmware/m32p/        │             │  3. build PDFs          │
   │   versions.json (×2)    │             │  4. gh release create   │
   └─────────────────────────┘             │  5. write to Dropbox    │
              │ syncs                      └─────────────────────────┘
              ▼
       site44 → www.morserino.info/firmware
```

**Why self-hosted runner on the Mac (option D3-c)?**

- Direct filesystem access to the Dropbox folder. No API token to manage, no extra failure surface.
- The macOS extended-attribute issue (PlatformIO outputs get the macOS `com.apple.quarantine`/Finder hidden flag) is handled with the same `xattr -c` step you currently run manually.
- The Mac will be on whenever you tag a release (you only release from the Mac).
- Trade-off: the Mac must be online for the workflow to fire. Acceptable per developer constraint.

---

## 4. Tag scheme

Driven by **the tag**, which is the single source of truth.

### Stable

```
V<major>.<minor>[.<patch>]

examples:  V8.1   V8.2.1   V9.0
```

- Workflow extracts `<major>.<minor>[.<patch>]` as the version string.
- `prerelease: false` on the GitHub Release.
- Marked as **latest** by GitHub.

### Beta

```
V<major>.<minor>[.<patch>]-beta.<counter>

examples:  V8.1-beta.1   V8.1-beta.2   V8.2.1-beta.7
```

- The counter (`.1`, `.2`, …) exists **only to make the tag unique on GitHub**. It does not normally appear in filenames or manifest entries.
- The build date (`<YYMMDD>` of the day the workflow runs) is stamped into the filename and manifest version string.
- `prerelease: true` on the GitHub Release.
- Common case: `V8.1-beta.1` → `fw_m32_V8.1_beta_260601.bin`, manifest `"8.1 beta 260601"`.
- Same-day collision case: see §5.

### Regex used by the workflow

```
stable:  ^V(\d+\.\d+(?:\.\d+)?)$
beta:    ^V(\d+\.\d+(?:\.\d+)?)-beta\.(\d+)$
```

A tag that matches neither pattern is rejected with a clear error and no release is created.

---

## 5. Filename conventions

The workflow renames PlatformIO output files using exactly these rules:

| Source | Target (stable) | Target (beta, no collision) | Target (beta, same-day collision) |
|---|---|---|---|
| `Software/src/.pio/build/heltec_wifi_lora_32_V2/firmware.bin` | `fw_m32_V<ver>.bin` | `fw_m32_V<ver>_beta_<YYMMDD>.bin` | `fw_m32_V<ver>_beta_<YYMMDD>.<N>.bin` |
| `Software/src/.pio/build/pocketwroom/firmware.bin` | `fw_m32p_V<ver>.bin` | `fw_m32p_V<ver>_beta_<YYMMDD>.bin` | `fw_m32p_V<ver>_beta_<YYMMDD>.<N>.bin` |

The case of `V` in filenames matches the existing convention (uppercase).

### Same-day beta collision rule

When two betas of the same version are released on the same day, the second (and any subsequent) one gets a suffix using **the tag's own counter** — never an auto-incremented value:

| Tag | Existing in Dropbox | Filename | Manifest version |
|---|---|---|---|
| `V8.1-beta.1` | none | `fw_m32_V8.1_beta_260601.bin` | `"8.1 beta 260601"` |
| `V8.1-beta.2` (next day) | yesterday's beta | `fw_m32_V8.1_beta_260602.bin` | `"8.1 beta 260602"` |
| `V8.1-beta.2` (same day as `.1`) | morning's beta | `fw_m32_V8.1_beta_260601.2.bin` | `"8.1 beta 260601 #2"` |
| `V8.1-beta.3` (same day as `.1` & `.2`) | morning + afternoon | `fw_m32_V8.1_beta_260601.3.bin` | `"8.1 beta 260601 #3"` |

The collision check is performed independently per platform (M32 vs Pocket), against both the Dropbox folder contents **and** the existing `versions.json` entries. If either source shows the would-be filename or version string already exists, the suffix is added.

**Caveat:** if you manually delete a same-day beta from Dropbox between two pushes, the workflow will not detect the collision and the second beta will get the clean filename. This is arguably correct (deletion = intent to replace) but worth knowing.

---

## 6. PDF handling

The workflow always **rebuilds the PDFs from markdown** at release time (B2: "be on the safe side").

For each tag, the workflow:

1. Computes `<major>` from the version (e.g. `V8.1-beta.1` → `8`).
2. `cd "Documentation/User Manual/Version <major>.x"` and runs `./build.sh`.
3. Picks up the resulting `m32UserManual_v<major>_en.pdf` and `m32UserManual_v<major>_de.pdf`.

### Pocket FAQ

The Pocket FAQ at `Documentation/FAQ/Morserino-32 Pocket FAQ.pdf` should ideally also be rebuilt from `Morserino-32 Pocket FAQ.md`, but **there is no `build.sh` for the FAQ today** — the PDF has been produced by hand.

**Plan:**
- **Phase 1 (initial release-workflow rollout):** the FAQ PDF is attached as-is from the committed copy. The workflow logs a warning if the `.md` is newer than the `.pdf`.
- **Phase 2 (small follow-up):** add a `build.sh` under `Documentation/FAQ/` (modelled on the User Manual one — pandoc + the same Lua filter where applicable). Once it exists, the workflow rebuilds the FAQ alongside the manuals.

This keeps the initial pipeline focused while not losing track of the goal.

### pandoc

`build.sh` uses pandoc + a Lua filter (`normalize_ids.lua`). pandoc is already installed on the developer's Mac at `/usr/local/bin/pandoc` (Homebrew on Intel). The runner inherits the developer user's `PATH`, so this is available without extra setup. Verified in §12.

---

## 7. Release notes / changelog

The developer maintains `Software/README.md` (a `# Change History` document) by hand and commits the new section **before** tagging.

The workflow extracts the section for the version being released and uses it as the body of the GitHub Release.

### Extraction algorithm

1. Open `Software/README.md`.
2. Look for the first heading that matches a tolerant regex:
   ```
   ^###\s+Changes\s+V\.?\s*<escaped-version>\s*$
   ```
   This matches all three observed forms: `### Changes V. 8.0`, `### Changes V.7.1`, `### Changes V.7.0`.
3. Capture all lines until the next `### ` heading (or EOF).
4. Use that block as the release body.

### Failure modes

- **No matching section found:** workflow **fails fast** with `Error: no '### Changes V<ver>' section found in Software/README.md. Update the changelog and re-tag.` No release is created.
- **Empty section:** treated as a soft warning; release is created with body "_No notes provided._"

For betas: the workflow looks for `### Changes V<ver>` (without the beta counter suffix). All betas of V8.1 share the cumulative `### Changes V.8.1` section, which grows as the release approaches. This matches how the README is maintained today.

---

## 8. GitHub Release

Created via `gh release create` from the runner.

| Property | Stable | Beta |
|---|---|---|
| Tag name | `V8.1` | `V8.1-beta.1` |
| Release title | `Version 8.1` | `Version 8.1 beta 260601` (or `… 260601 #2` on collision) |
| Pre-release flag | `false` | `true` |
| Latest flag | `true` | `false` |
| Body | Extracted from README | Same section, plus a `> Beta build — for testing` banner |

### Assets attached

| Asset | Source |
|---|---|
| `fw_m32_V<ver>[_beta_<date>].bin` | built |
| `fw_m32p_V<ver>[_beta_<date>].bin` | built |
| `m32UserManual_v<major>_en.pdf` | rebuilt from md |
| `m32UserManual_v<major>_de.pdf` | rebuilt from md |
| `Morserino-32 Pocket FAQ.pdf` | committed copy |

The release is created as a **draft**. The developer reviews everything (binaries, manuals, body text, asset filenames) in the GitHub UI and clicks **Publish** to make it visible. This safety net is appreciated during the workflow's first runs; once it's clearly stable we can switch to immediate-publish by flipping one flag in `release.yml`.

---

## 9. Dropbox / web-installer publication

Local paths the workflow writes into:

```
/Users/wkraml/Library/CloudStorage/Dropbox/Apps/site44/www.morserino.info/firmware/
├── fw_m32_V<ver>[_beta_<date>].bin       ← new copy
├── versions.json                          ← prepend new entry
└── m32p/
    ├── fw_m32p_V<ver>[_beta_<date>].bin   ← new copy
    └── versions.json                      ← prepend new entry
```

The Dropbox client picks up the file system writes and syncs them to site44.

### Manifest entry format

For stable:
```json
{
  "version": "8.1",
  "filename": "fw_m32_V8.1.bin",
  "notes": "Release of V.8.1",
  "beta": false
}
```

For beta:
```json
{
  "version": "8.1 beta 260601",
  "filename": "fw_m32_V8.1_beta_260601.bin",
  "notes": "Beta of V.8.1 dated 260601",
  "beta": true
}
```

### Manifest update rules

- **Read** the existing `versions.json` (array).
- **Prepend** the new entry at the top.
- **Do not** modify, deduplicate, reorder, or rewrite any existing entries — preserves your historical entries verbatim, including pre-existing typos like `"8.0 beta 2604027"` in the current file. (You can clean those up by hand any time.)
- **Do not** auto-replace duplicate `filename`s. If a duplicate filename is detected, the workflow fails with a clear error rather than silently overwriting.
- Write atomically (write to `versions.json.tmp` in the same folder, then `mv`) to avoid Dropbox syncing a half-written file.

### Notes field

The `notes` field is somewhat freeform in the existing manifests ("Released March 9, 2026", "first version with esp-now", "Sixth beta of V.8"). The workflow supports two sources for it:

- **If the tag is annotated and has a message** — i.e. you pushed `git tag -a V8.1 -m "Release of V.8.1 with foo"` — the message is used verbatim as the `notes` field.
- **Otherwise (lightweight tag, or annotated tag with empty message)** — a templated default:
  - Stable: `"Release of V.<ver>"`
  - Beta: `"Beta of V.<ver> dated <YYMMDD>"` (or `"… dated <YYMMDD> #<N>"` on same-day collision)

This lets the tag be the single source of truth: when you want a custom blurb, use `-a -m`; otherwise just `git tag V8.1` and accept the default.

The same `notes` value is used in both `firmware/versions.json` and `firmware/m32p/versions.json` (kept in sync — there's no facility to give the two platforms different notes for the same release).

---

## 10. Workflow stages — pipeline order

```
┌─────────────────────────────────────────────────────────────────┐
│ 1.  Parse tag → version, date (if beta), build channel          │
├─────────────────────────────────────────────────────────────────┤
│ 2.  Verify changelog: ensure '### Changes V<ver>' exists        │
│     (fail fast if not — no point building)                      │
├─────────────────────────────────────────────────────────────────┤
│ 3.  Build heltec_wifi_lora_32_V2 (pio run -e ...)               │
├─────────────────────────────────────────────────────────────────┤
│ 4.  Build pocketwroom (pio run -e ...)                          │
├─────────────────────────────────────────────────────────────────┤
│ 5.  Strip macOS xattrs from both .bin files                     │
├─────────────────────────────────────────────────────────────────┤
│ 6.  Rename binaries per convention into a staging dir           │
├─────────────────────────────────────────────────────────────────┤
│ 7.  Rebuild PDFs for major version                              │
├─────────────────────────────────────────────────────────────────┤
│ 8.  Stage Pocket FAQ PDF                                        │
├─────────────────────────────────────────────────────────────────┤
│ 9.  Extract release notes from README                           │
├─────────────────────────────────────────────────────────────────┤
│ 10. Create GitHub Release (draft, with all assets attached)     │
├─────────────────────────────────────────────────────────────────┤
│ 11. Copy renamed .bin files into Dropbox folders                │
├─────────────────────────────────────────────────────────────────┤
│ 12. Update versions.json for each platform (atomic prepend)     │
├─────────────────────────────────────────────────────────────────┤
│ 13. STABLE ONLY: copy binaries to in-repo archive, commit to    │
│     default branch, push (with [skip ci])                       │
├─────────────────────────────────────────────────────────────────┤
│ 14. Summary comment posted on the workflow run                  │
└─────────────────────────────────────────────────────────────────┘
```

Any failure short-circuits and leaves the system in a clean state (no partial uploads, no partial manifest edits — the GitHub Release is created late in the sequence so Dropbox-side problems can't leave dangling release artifacts).

### In-repo binary archive (stage 13)

For stable releases the workflow commits the freshly-built binaries into the long-standing in-repo archive so users who pull from git keep finding them:

```
Software/binary/From Version 7 onwards/
├── M32 1st & 2nd edition/
│   └── fw_m32_V<ver>.bin       ← new copy added here
└── M32 Pocket/
    └── fw_m32p_V<ver>.bin       ← new copy added here
```

- **Stable releases only** — matches the existing pattern (no betas in this archive).
- Target branch: the repo's **default branch** (`master`), auto-detected via `gh repo view`. The tag may point at a non-tip commit; the workflow checks out the default branch, copies the binaries there, commits, and pushes. This means the archive grows from the default branch's HEAD, not from the tagged commit — and the result is reachable from `master` for anyone cloning.
- Commit message: `Add V<ver> binaries to archive [skip ci]`. The `[skip ci]` marker prevents `pio-ci.yml` from re-firing on the binary-only commit.
- Author: `github-actions[bot]`.
- If the push is rejected (someone else pushed to master between fetch and push), the workflow fails. Re-running it picks up the latest tip and tries again. Acceptable trade-off vs. silent retries that mask conflicts.

---

## 11. File structure to be created

```
.github/workflows/
  release.yml              ← new — main release workflow
  pio-ci.yml               ← unchanged (per-push smoke test)

scripts/
  release/
    parse_tag.py           ← tag → version, date, channel
    extract_changelog.py   ← README → notes for this version
    rename_binaries.py     ← + xattr stripping
    update_manifest.py     ← prepend entry to versions.json
    dropbox_publish.sh     ← orchestrates the file copy + manifest update
  clean_ino_dupes.py       ← unchanged (existing pre-build hook)
```

All Python scripts:

- Run on Python 3.11+ (matches existing `pio-ci.yml`).
- Have a `--dry-run` flag that logs what they would do without writing anything.
- Take their inputs as explicit CLI arguments (no implicit env-var magic), so they can be tested by hand from a terminal.

---

## 12. Self-hosted runner — one-time setup

A separate prep task, **documented** in a sibling file `RUNNER_SETUP.md` (not scripted — it's a once-per-machine thing).

The Mac needs:

1. GitHub Actions runner installed and registered with the repo, with label `macos-morserino`.
2. Runner configured as a **launchd-managed background service** so it starts at login.
3. `pandoc` available on `PATH`. Already present at `/usr/local/bin/pandoc`. Verify with `which pandoc` before first run.
4. PlatformIO available on `PATH` (`pip install platformio`, or whichever method the developer already uses for local builds).
5. `gh` CLI authenticated as `oe1wkl`.
6. Environment variable `MORSERINO_DROPBOX_ROOT` exported in the runner's environment (see §13).
7. Confirm the runner has write access to the Dropbox folder (it will, since it runs as the developer's user).

---

## 13. Required secrets / config

**Secrets:** none at the GitHub repo level beyond the automatic `GITHUB_TOKEN`. All file-system writes happen on the local Mac. Confirmed today with `gh secret list` (returned empty).

**Runner environment variable:** `MORSERINO_DROPBOX_ROOT` — points at the Dropbox firmware root, currently:

```
/Users/wkraml/Library/CloudStorage/Dropbox/Apps/site44/www.morserino.info/firmware
```

The workflow scripts read this variable; if it is unset or points to a non-existent directory the workflow fails fast with a clear error before doing any work. Keeping it as an env var (rather than hard-coding the path in YAML) means the design isn't tied to one machine and the path can be moved without editing the workflow.

---

## 14. Existing `pio-ci.yml` — what changes (nothing)

Stays exactly as it is per E2. Builds `heltec_wifi_lora_32_V2` on every push to verify that pocketwroom-focused development hasn't broken the original M32 build. No interaction with the new release workflow.

---

## 15. Concurrency, rollbacks, edge cases

- **Concurrency group:** `release-${{ github.ref }}` — two pushes of the same tag cannot run in parallel.
- **Duplicate tag push:** if a GitHub Release for that tag already exists, the workflow fails fast (rather than re-uploading and risk-duplicating manifest entries).
- **Manifest dedup:** if the new `filename` already appears in `versions.json`, fail fast — the developer should bump the date or version.
- **PDF rebuild fails:** workflow fails before creating the GitHub Release. Dropbox is untouched.
- **Dropbox copy fails (disk full, permission, etc.):** workflow fails after the GitHub Release is created. The release exists; the web installer doesn't list the new version yet. Recovery: re-run the manifest-update script by hand.
- **Tag pushed without an updated changelog section:** fails at stage 2 before any build runs.
- **In-repo archive push rejected (race with another push to master):** workflow fails at stage 13. Earlier side effects (draft release, Dropbox files, manifest entries) are intact. Recovery: re-run the workflow, or manually copy the binaries to `Software/binary/From Version 7 onwards/…` and commit by hand.

---

## 16. Decisions log

All previously open questions are now resolved:

| ID | Question | Decision |
|---|---|---|
| P1 | Rebuild Pocket FAQ PDF, or copy committed file? | Copy committed file in Phase 1; add `Documentation/FAQ/build.sh` and rebuild in Phase 2 |
| P2 | `pandoc` availability for `build.sh` | Confirmed at `/usr/local/bin/pandoc` |
| N1 | Beta release notes — share with stable section, or per-beta? | Share with the cumulative `### Changes V<ver>` section |
| R1 | GH Release created as draft or auto-published? | Draft; flip to auto-publish later if desired |
| M1 | Override the manifest `notes` field at tag time? | Annotated tag message (`git tag -a … -m "…"`) wins; otherwise templated default |
| S1 | Script or document runner setup? | Document, in `RUNNER_SETUP.md` |
| C1 | Hard-code Dropbox path or env var? | Env var `MORSERINO_DROPBOX_ROOT` |
| **A3 (revised)** | Beta tag scheme | `V<ver>-beta.<counter>`; date stamped at build time; same-day collisions get a `.<counter>` filename suffix (see §5) |

Design is **locked**. Implementation can proceed:
1. `RUNNER_SETUP.md` written and runner registered.
2. `scripts/release/` Python helpers written, each unit-testable with `--dry-run`.
3. `.github/workflows/release.yml` written, pointing at the self-hosted runner.
4. First end-to-end dry-run with a throwaway tag (e.g. `V0.0-beta.1` on a scratch branch) to validate the whole pipeline before retiring the manual process.

---

## 17. Out of scope for this design / future ideas

Noting these so we don't forget but don't try to do them now:

- `manifest_current_m32p.json` (the ESPHome-style multi-part manifest at `firmware/manifest_current_m32p.json`) — needs separate treatment. Used by a different installer flow? To be discussed once the basic pipeline is in.
- Auto-publish to other channels (Twitter/Mastodon/forum) on release.
- Signed firmware / secure boot.
- Cleanup of older beta artifacts in Dropbox.
- A `release-dry-run` workflow on PRs that simulates everything without publishing.
