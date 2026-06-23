# RELEASING(1) — Morserino-32 release quick reference

> Companion to [RELEASE_AUTOMATION_DESIGN.md](RELEASE_AUTOMATION_DESIGN.md)
> (design rationale) and [RUNNER_SETUP.md](RUNNER_SETUP.md) (runner setup).
> This file is the one to read when you actually want to ship.

---

## NAME

**morserino-release** — produce a beta or stable firmware release.

## SYNOPSIS

```
git tag <TAG>
git push origin <TAG>
```

where `<TAG>` is one of:

```
V<major>.<minor>[.<patch>]                  (stable release)
V<major>.<minor>[.<patch>]-beta.<counter>   (beta release)
```

## DESCRIPTION

Pushing a `V*` tag fires `.github/workflows/release.yml` on the
self-hosted macOS runner (`morserino-mac`). The workflow builds
firmware for both platforms, rebuilds the user manuals, creates a
**draft** GitHub Release with all assets attached, publishes the
firmware to the Dropbox folder backing the web installer, updates
both `versions.json` manifests, and (for stable releases only)
commits the new binaries to the in-repo archive on `master`.

The release is **always created as a draft**. You must review and
**publish** it manually from the GitHub web UI.

## PRE-FLIGHT (do this before tagging)

Before you tag, make sure of these — the workflow fails fast on each:

1. **Update `Software/README.md`.** Add a section for the new
   version under `# Change History`. The heading must match the
   version you're about to tag. All of these forms work
   (case-insensitive, optional space after `V.`):

   ```markdown
   ### Changes V8.1
   ### Changes V. 8.1
   ### Changes V.8.1
   ### CHANGES V. 8.1
   ```

   For beta releases, write the cumulative notes for `V<ver>`; every
   beta of that version (`V8.1-beta.1`, `V8.1-beta.2`, …) reuses the
   same section. You don't need a separate section per beta.

2. **Commit and push** the README change to `master`:
   ``` 
   sh
   git add Software/README.md
   git commit -m "Update V<ver> changelog"
   git push origin master
   ```

3. **The Mac (with the runner) is on and not asleep.** The runner is a
   self-hosted job and won't pick up the workflow until it's online.

4. *(For docs that changed.)* If you edited `manual_en.md` /
   `manual_de.md`, also push those before tagging. The workflow
   rebuilds the PDFs anyway, but the stage-13 doc-sync step decides
   whether to commit the rebuilt PDFs to `master` by comparing the
   `.md` against `master`'s last PDF commit. Your edited `.md` must
   be on `master` for that comparison to fire.

## TAG FORMAT — DETAILS

### Stable

```
V8.1            → version "8.1",   major 8
V8.2.1          → version "8.2.1", major 8
V9.0            → version "9.0",   major 9
```

Tag matches this regex:  `^V(\d+\.\d+(?:\.\d+)?)$`

Anything else is rejected by the workflow with a clear error.

### Beta

```
V8.1-beta.1     → version "8.1", counter 1, date = today (YYMMDD)
V8.1-beta.2     → version "8.1", counter 2  (later beta of same version)
V8.2.1-beta.3   → version "8.2.1", counter 3
```

Tag matches this regex:  `^V(\d+\.\d+(?:\.\d+)?)-beta\.(\d+)$`

The counter (`.1`, `.2`, …) is only there to make the tag unique on
GitHub — it does **not** appear in the filename or manifest unless
there's a same-day collision (see below). The build date is what
gets stamped into the filename.

### Same-day beta collision

If you push more than one beta of the same version on the same day
(rare), the second one gets the counter as a suffix:

```
V8.1-beta.1 pushed today (e.g. 260615)
   → fw_m32_V8.1_beta_260615.bin

V8.1-beta.2 also pushed today (collision with .1)
   → fw_m32_V8.1_beta_260615.2.bin
   → manifest version: "8.1 beta 260615 #2"
```

You never have to compute this yourself — the workflow detects the
collision and adds the suffix automatically.

## OPTIONAL: ANNOTATED TAG WITH CUSTOM MANIFEST NOTES

By default, the `notes` field in `versions.json` is a templated string:

```
stable:  "Release of V.<ver>"
beta:    "Beta of V.<ver> dated <YYMMDD>"
```

To override the notes with custom text, push an **annotated** tag
with `-a -m "..."`. The message becomes the `notes` field:

```sh
git tag -a V8.1 -m "Long-awaited release with the new game engine"
git push origin V8.1
```

Lightweight tags (`git tag V8.1`, no `-a -m`) use the templated
default.

## STANDARD RELEASE PROCEDURE

### Beta

```sh
# 1. Pre-flight (changelog committed, runner online, etc.)
git pull --ff-only origin master

# 2. Tag and push.
git tag V<ver>-beta.<N>
git push origin V<ver>-beta.<N>

# 3. Wait ~5 minutes. Watch:
#    https://github.com/oe1wkl/Morserino-32/actions
#    OR  gh run watch -R oe1wkl/Morserino-32

# 4. Review the draft release. Open it in the web UI:
#    https://github.com/oe1wkl/Morserino-32/releases
#    Sanity-check that all 5 assets are attached:
#      - fw_m32_V<ver>_beta_<date>.bin
#      - fw_m32p_V<ver>_beta_<date>.bin
#      - m32UserManual_v<major>_en.pdf
#      - m32UserManual_v<major>_de.pdf
#      - Morserino-32.Pocket.FAQ.pdf
#    Edit the body text if needed (it was filled in from the README).

# 5. Click "Publish release".

# That's it. Web installer + Dropbox already have the new firmware
# (the workflow updated them before creating the draft).
```

### Stable

Exactly the same — just a different tag format:

```sh
git pull --ff-only origin master
git tag V<ver>             # e.g. V8.1
git push origin V<ver>

# ... wait, review draft, publish. ...
```

For stable releases, you'll see TWO extra commits land on `master`
when the workflow finishes:

```
Add V<ver> binaries to archive [skip ci]
Update V<ver> docs (rebuilt from sources) [skip ci]   (only if .md changed)
```

Run `git pull` next time you do any work to bring them down.

## WHAT THE WORKFLOW DOES

In order (per [RELEASE_AUTOMATION_DESIGN.md §10](RELEASE_AUTOMATION_DESIGN.md)):

```
 1. Parse tag → version, channel (stable/beta), date, counter
 2. Verify changelog: '### Changes V<ver>' must exist in README
 3. Read annotated-tag message (for custom manifest notes)
 4. Build heltec_wifi_lora_32_V2  (original M32)
 5. Build pocketwroom               (M32 Pocket)
 6. Strip macOS xattrs from binaries
 7. Rename binaries per convention into a staging dir
 8. Rebuild User Manual PDFs from .md (pandoc → weasyprint)
 9. Stage Pocket FAQ
10. Compose release body from extracted README section
11. Create DRAFT GitHub Release; upload 5 assets serially
12. Copy binaries to Dropbox + prepend versions.json entries
13. Sync rebuilt docs to master IF .md changed since last PDF commit
14. STABLE ONLY: commit binaries to in-repo archive on master
15. Summary
```

Any step failure short-circuits the rest. The Mac runner must be on
the whole time (~5 min).

## ENVIRONMENT

Set on the runner (in `~/actions-runner/.env`):

```
MORSERINO_DROPBOX_ROOT=/Users/wkraml/Library/CloudStorage/Dropbox/Apps/site44/www.morserino.info/firmware
```

The runner's PATH must include:
- `pandoc` (Homebrew: `/usr/local/bin/pandoc` on Intel, `/opt/homebrew/bin/pandoc` on ARM)
- `weasyprint`
- `pio` (PlatformIO)
- `gh` (GitHub CLI, authenticated as `oe1wkl`)

The **Lato** font must be installed for PDF rendering.

See [RUNNER_SETUP.md](RUNNER_SETUP.md) for full setup details.

## FILES THE WORKFLOW READS

```
.github/workflows/release.yml          ← the workflow itself
Software/README.md                     ← changelog source (must contain '### Changes V<ver>')
Software/src/platformio.ini            ← build configuration
Software/src/.pio/build/<env>/firmware.bin  ← compiled output
Documentation/User Manual/Version <major>.x/
    manual_en.md / manual_de.md        ← PDF sources
    build.sh                           ← invoked to rebuild PDFs
Documentation/FAQ/Morserino-32 Pocket FAQ.pdf
```

## FILES THE WORKFLOW WRITES

```
$MORSERINO_DROPBOX_ROOT/
    fw_m32_V<ver>...bin                ← copied here
    versions.json                      ← prepended
    m32p/
        fw_m32p_V<ver>...bin           ← copied here
        versions.json                  ← prepended

$REPO/Documentation/User Manual/Version <major>.x/
    m32UserManual_v<major>_{en,de}.pdf ← committed to master if .md changed
    manual_{en,de}.html

$REPO/Software/binary/From Version 7 onwards/   (stable only)
    M32 1st & 2nd edition/fw_m32_V<ver>.bin
    M32 Pocket/fw_m32p_V<ver>.bin
```

## TROUBLESHOOTING — IF THE WORKFLOW FAILS

The workflow's stages are ordered "least to most destructive," so
where it fails tells you what's still safe.

| Stage failed | What's untouched | What needs cleanup |
|---|---|---|
| 1 — Parse tag | everything | delete the bad tag: `git tag -d <T>; git push origin --delete <T>` |
| 2 — Verify changelog | everything | add `### Changes V<ver>` to README, commit, re-tag |
| 4–5 — Build | everything | check `pio` works locally on the same env |
| 8 — PDFs | everything | check `pandoc` / `weasyprint` / Lato on the runner |
| 11 — Create release | GH may have a stale draft | `gh release delete <T> --cleanup-tag` then re-tag |
| 12 — Dropbox publish | repo untouched | delete the draft release; manually clean Dropbox files + manifest top entry; re-tag |
| 13 — Doc sync | release + Dropbox done | re-run workflow (it's idempotent) OR commit docs by hand |
| 14 — Archive (stable) | everything else done | re-run workflow OR commit binaries to archive by hand |

### Quick rollback recipe for a botched beta

```sh
# Replace YYMMDD with the date that was stamped into the filename
TAG=V<ver>-beta.<N>
DATE=YYMMDD
DROPBOX_ROOT="/Users/wkraml/Library/CloudStorage/Dropbox/Apps/site44/www.morserino.info/firmware"

gh release delete "$TAG" -R oe1wkl/Morserino-32 --yes --cleanup-tag
git tag -d "$TAG" 2>/dev/null || true

rm -f "$DROPBOX_ROOT/fw_m32_V<ver>_beta_${DATE}.bin"
rm -f "$DROPBOX_ROOT/m32p/fw_m32p_V<ver>_beta_${DATE}.bin"

# Remove the top entry from each manifest (asserts top entry IS the bad one)
EXPECTED="<ver> beta ${DATE}"
for m in "$DROPBOX_ROOT/versions.json" "$DROPBOX_ROOT/m32p/versions.json"; do
    python3 -c "
import json, os, sys
p = sys.argv[1]; expected = sys.argv[2]
data = json.load(open(p))
assert data[0]['version'] == expected, f'Top entry is {data[0][\"version\"]!r}, not {expected!r}'
tmp = p + '.tmp'
json.dump(data[1:], open(tmp, 'w'), indent=2, ensure_ascii=False)
open(tmp, 'a').write('\n')
os.replace(tmp, p)
" "$m" "$EXPECTED"
done
```

For a botched **stable** rollback you also need to revert any commits
on `master` whose subject starts with `Add V<ver> binaries to archive`
or `Update V<ver> docs`. Use `git revert <sha>` for each.

## SEE ALSO

- [RELEASE_AUTOMATION_DESIGN.md](RELEASE_AUTOMATION_DESIGN.md) — full design, decision log, and architecture
- [RUNNER_SETUP.md](RUNNER_SETUP.md) — one-time runner installation
- [HANDOFF.md](HANDOFF.md) — project state-of-affairs

## EXAMPLES

```sh
# First beta of V8.1 (assumes V8.1 changelog is in README and committed)
git tag V8.1-beta.1 && git push origin V8.1-beta.1

# Second beta a week later
git tag V8.1-beta.2 && git push origin V8.1-beta.2

# Stable V8.1 (lightweight tag; templated 'notes' field)
git tag V8.1 && git push origin V8.1

# Stable V8.2.1 (annotated tag with custom manifest notes)
git tag -a V8.2.1 -m "Critical bug-fix release for V8.2"
git push origin V8.2.1

# Bug-fix patch release V8.2.2
git tag V8.2.2 && git push origin V8.2.2
```
