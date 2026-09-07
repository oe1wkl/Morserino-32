# Upstreaming the vendored sidetone library — handoff

**Goal:** decide what of our patched `cw-i2s-sidetone` should go to
`haklein/cw-i2s-sidetone`, and — if enough of it does — stop vendoring.

**Status 2026-09-07:** one fix upstreamed and merged. The rest is unexamined.
Willi now has **direct write access** to Hari's repository, so the fork-and-PR
dance is optional; a branch and a PR there is still the courteous shape for
anything substantial.

---

## Where things stand

Our copy lives at `Software/src/vendor/cw-i2s-sidetone/`, referenced from
`Software/src/platformio.ini` as `symlink://vendor/cw-i2s-sidetone` (deliberately
Pocket-scoped — the classic/OLED build must not pull it). It replaced the
registry version when the V9 accessibility work needed async playback.

Upstream is at `36287a8` (2026-09-07), which is our merged PR #2.

| file | lines differing from upstream | what it is |
|---|---|---|
| `src/I2S_Sidetone.cpp` | 136 | async clip API + `setTimbre` |
| `include/ComplexRotorSine.hpp` | 118 | voiced timbres (`Timbre`, `Trumpet`, `Bassoon`, `Voice`/`Rotor`) |
| `include/BlackmanHarrisEnvelope.hpp` | 35 | configurable envelope behind the Tone Softness preference |
| `include/I2S_Sidetone.hpp` | 28 | declarations for the above |
| `library.json` | 16 | our `1.0.5+m32.1` marker — must NOT go upstream |

Symbols that exist only in our copy: `startClip`, `stopClip`, `serviceClip`,
`isClipPlaying`, `clipsPlayed`, `audioLoop`, `teardownClip`, `setTimbre`.

### Already upstream (do not re-send)

* **PR #1** — silencing Info-level logging in `begin()`.
* **PR #2** — `playSPIFFSFile()` waiting for the pipeline rather than the file.
  Merged 2026-09-07. **The logic is now character-identical on both sides**; our
  copy differs only by the `clipBusy` guard above it, which belongs to the async
  work, and by a longer comment that references our click investigation. When
  the two are next reconciled, upstream's version of that hunk can simply be
  taken.

---

## The three candidates, and what each really costs

### 1. Configurable envelope (`BlackmanHarrisEnvelope`, ~35 lines)

The smallest and least entangled. `setADSR()` plus the plumbing behind the Tone
Softness preference (PR #196, ckonecny). Nothing M32-specific in the mechanism —
upstream's envelope is fixed, ours is settable.

**Verdict: send it.** Small, self-contained, obviously general.

### 2. Selectable timbres (`ComplexRotorSine`, ~118 lines)

`enum class Timbre` with `Trumpet`, `Bassoon` and friends, plus `setTimbre()`.
The *mechanism* is general — a rotor-based oscillator that can be given a partial
recipe. The *recipes* were chosen for the Pocket's micro-speaker, after PR #208
de-clipped the DAC and left the pure sine inaudible.

**Verdict: offer the mechanism, be honest that the recipes are tuning.** Hari may
want the enum without our specific voices, or may not want the enum at all if his
users only ever need a sine.

### 3. Async clip playback (~136 lines, the bulk)

`startClip`/`stopClip`/`serviceClip`/`isClipPlaying`/`clipsPlayed`, plus
`audioLoop()` and `teardownClip()` in the audio task, a 1-deep command mailbox,
and a position watchdog.

This is the hard one, and the reason to think before pushing:

* **It changes the threading model.** Upstream's audio task just calls
  `copier->copy()`. Ours owns the clip lifecycle — file open/close, mixer
  routing, decoder reuse — strictly sequenced with `copy()`.
* **It encodes scar tissue.** The decoder is REUSED via `setStream()` and never
  `end()`/`begin()`-reset, because every cross-task reset attempt froze or
  crashed. `teardownClip()` fires the moment the file is drained because holding
  the mixer past EOF wedged the pipeline. Those constraints are real but they
  are ours; upstream would inherit them without the history that explains them.
* **It carries a known latent bug.** `audioLoop()` tears the clip down on
  `!mp3file.available()`, which is the same "file drained ≠ audio finished" error
  PR #2 just fixed in the blocking path — so async clips lose their last ~80 ms
  too. It does not bite us because `generate_audio.sh` pads every voice clip with
  ≥100 ms of digital silence (verified: 40 of 503 sampled, every one at
  −240 dBFS), so the truncation lands in the padding. **Do not upstream this
  without either fixing that or documenting it**, and be careful: the a11y
  edition's speech runs on this path and a freeze there is far worse than a
  clipped syllable.

**Verdict: do not push as-is.** Either fix the truncation first and offer it as a
considered feature, or describe it to Hari and let him decide whether he wants
that responsibility.

---

## Suggested sequence

1. **Ask Hari what he wants.** He has merged two of our PRs; the async design is
   a bigger commitment and he should get to choose rather than receive it.
2. **Send the envelope change** as its own PR — smallest, cleanest, no argument.
3. **Discuss the timbre mechanism**, separating it from our recipes.
4. **Decide about async clips** — fix the truncation first if it goes at all.
5. **Only then consider dropping the vendoring.** It is worth doing when (and
   only when) upstream carries everything we need; until then the symlink dep is
   the honest arrangement and `1.0.5+m32.1` says so.

## Things not to get wrong

* **Never send `library.json`.** Ours carries `1.0.5+m32.1` and a description
  saying it is a patched copy. Upstream's version is upstream's to set.
* **Rebuild all three shipping environments after any library change** —
  `pocketwroom`, `heltec_wifi_lora_32_V2`, `pocketwroom-accessibility` — from
  `Software/src/`. The classic build does not use this library but must still
  compile.
* **The a11y edition is the risk surface.** Anything touching `audioLoop()` /
  `teardownClip()` can freeze spoken menus for blind users. Bench it, do not
  reason about it.

## Related

* `devdocs/audio-accessibility/HANDOFF.md` — the async clip work's own history.
* `devdocs/signal-tones/FINDINGS.md` and `make_jingles.py` — the speaker model,
  the ≥290 Hz floor, and the jingle sealing that the 2026-09-07 investigation
  produced.
* `Software/src/platformio.ini` — the vendoring rationale, in a comment above
  `M32_Pocket_board_lib_deps`.
