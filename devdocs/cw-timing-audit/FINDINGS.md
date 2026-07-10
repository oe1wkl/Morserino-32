# CW element/gap timing audit — findings

**Status: FIXES APPLIED 2026-07-01, awaiting on-device re-measurement.**
Surfaced while building Fox Hunt (`devdocs/grid-games/`), on Willi's
real-hardware listening test, then narrowed with recorded-audio measurement
and Willi's own Audacity measurement. Two fixes landed (see "Resolution"
below); a third reported symptom was measured to be a non-issue. The
resolution section is the current truth — the hypothesis sections below are
kept as the reasoning trail.

## Resolution (2026-07-01)

- **Plain CW Keyer mode is correct — no change.** Willi re-measured it in
  Audacity: 2 dits + 2 gaps = 0.244s at 20 WPM vs. 0.240s ideal, <2% error.
  The dit-slightly-longer-than-gap asymmetry is just the off-ramp: audible
  perception doesn't start/end at zero amplitude, so they *sound* equal. This
  supersedes the confused row-3 measurement below (my envelope script wasn't
  precise enough across differently-leveled recordings). The CW Keyer is the
  **reference** for correct timing.
- **Hypothesis A → fixed.** `MorseCwEngine`'s inter-element gap was
  `millis() + ditMs() - 1`... i.e. changed FROM `- 7` TO `- 1`
  (`MorseCwEngine.cpp`, the `if (toneOn)` branch of `playTick`). The `- 7`
  counted the ~6ms `pwmNoTone` ramp *against* the gap; the classic keyer's
  inter-element is `millis() + ditLength - 1` (`m32_v6.ino` INTER_ELEMENT),
  exactly 6ms longer — code analysis and Willi's "add 6ms" instruction
  converged on the same number. The tone-duration `- 7`s (the `'1'`/`'2'`
  cases) are **left as-is** — only the gap was short. Character and word gaps
  are built on top of the inter-element gap (the `'0'`/`' '` cases add to the
  gap this line already scheduled), so they're corrected by the same one-line
  change. **Shared engine — Pileup and Radio Cave get the same (strictly more
  spec-correct) spacing; spot-check their audio.**
- **Hypothesis B → fixed.** Fox Hunt and Trailblazer called `delay(10)`
  unconditionally every loop iteration, skewing both the keyer's own element
  timers (player's keying read too long in-game) and — for Fox Hunt —
  `MorseCwEngine`'s playback scheduling. Both now mirror the classic loop's
  "very tight loop while keying" pattern: `pollKeyedChar()` returns the
  keyer-busy flag (`doPaddleIambic`'s return), and the loop `continue`s
  (no delay, skips idle-only housekeeping) while keying. Fox Hunt additionally
  skips the delay while a clue is sounding (`if (!isPlaying()) delay(10)`), so
  playback timing stays tight too. `delay(10)` now only runs when fully idle.
- **Hypothesis C** (the "still weird" overlap) — not separately chased;
  expected to be reduced or gone now that A and B are fixed. Re-listen; only
  reopen if it persists.

## What was reported

Playing Fox Hunt, Willi noticed by ear:
1. The device's own played-clue dits sound closer together than they should
   — the gap is shorter than the dit.
2. His own keying (Iambic B), as monitored through the game, sounds like the
   gap between dits is slightly *longer* than the dit.
3. A residual "sounds a bit weird" quality specifically at the moment his
   keying and the device's clue playback overlap (even after the clue is
   made to cancel outright rather than pause-and-resume — see Fox Hunt's
   `pollKeyedChar()` fix in the same session).
4. A separate, useful reframe: a recording of the same paddle in plain **CW
   Keyer mode** (no game, no `MorseCwEngine` involved) — offered as a
   "this one sounds right" reference point.

## Measured evidence

Analyzed with a small script (`analyze_wav.py`, envelope-threshold tone/gap
detection — not kept in the repo, throwaway analysis tool) against three
recordings Willi supplied. All dits; no dahs in the analyzed segments.

| Recording | Context | dit length | gap length | gap should ≈ dit |
|---|---|---|---|---|
| `sample2.wav`, tones 0-2 | Device's own Fox Hunt clue playback | ~79ms | ~54ms | **31% short** |
| `sample2.wav`, tones 3-4 | Willi's own keying, *while playing Fox Hunt* | ~73.5ms | ~89ms | **21% long** |
| `sample3.wav` | Willi's own keying, **plain CW Keyer mode** (offered as "sounds right") | ~68-74ms | ~53-54ms | **~25-30% short**, i.e. numerically similar in direction/magnitude to the device's own Fox Hunt clue playback, *not* a clean 1:1 ratio by this measurement |

The third row is the important, slightly confusing one: by this
envelope-threshold measurement, the "sounds right" reference recording does
**not** actually show a clean gap≈dit ratio either — it looks quantitatively
similar to the device's own clue-playback asymmetry (#1), not perfectly
symmetric. Two explanations are both plausible and not yet distinguished:

- The three recordings were not level-matched (peak envelope ~4900-5100 for
  `sample.wav`/`sample2.wav` vs ~1800 for `sample3.wav` — a much quieter
  capture, likely different mic distance/gain), which can shift where a
  fixed relative-threshold crosses the attack/decay ramp asymmetrically
  between recordings. The measurement tool itself may not be precise enough
  to compare across differently-leveled recordings.
- There may be a real, small, universal gap-shortening built into
  `pwmTone`/`pwmNoTone` (see Hypothesis A) that's simply below Willi's
  perceptual threshold in plain Keyer mode, and only becomes objectionable
  in Fox Hunt once a second, larger effect (Hypothesis B) stacks on top of
  it and flips the net direction.

**Before drawing conclusions, this needs a controlled A/B recording** — same
mic position/distance, same gain, same WPM, back-to-back: plain Keyer mode
vs. Fox Hunt clue playback vs. Fox Hunt's own keying-through-the-game. The
three supplied samples were captured opportunistically during play-testing,
not for this comparison, and shouldn't be over-read numerically.

## Hypothesis A — `pwmTone`/`pwmNoTone`'s blocking "soften the click" ramp

`MorseOutput::pwmTone()` and `pwmNoTone()` (`MorseOutput.cpp:1994-2061`) each
contain a **blocking** ~6ms volume ramp in the non-I2S path (`ledcWrite(...);
delay(3); ledcWrite(...); delay(3);`) — deliberately added ("experimental:
soften the initial click") to avoid an audible click at full-volume on/off
transitions. The I2S path has an equivalent flat `delay(6)`.

`MorseCwEngine::playTick()` (`MorseCwEngine.cpp`) schedules its *next* event
by reading `millis()` **immediately after** these blocking calls return —
i.e. after ~6ms has already silently elapsed inside the very call establishing
the timing epoch — then applies its own `-7ms` "compensates for pwmNoTone
latency" adjustment on top. Worked through as a timeline (see the session
transcript for the full derivation), this predicts the tone-on phase coming
out a few ms *longer* than scheduled and the gap a few ms *shorter*, which
matches the *direction* of the device-playback asymmetry in row 1, though not
precisely the magnitude — there is likely some additional contributing factor
not yet identified (possibly non-linearity in how the volume ramp crosses an
envelope-detection threshold, possibly something else in the call chain not
yet traced).

**Blast radius if this needs a real fix:** `MorseCwEngine` is shared by Fox
Hunt, Pileup, and Radio Cave. `pwmTone`/`pwmNoTone` are used by literally
every CW-producing code path in the firmware (classic Generator, Echo
Trainer, the iambic keyer's own sidetone, games). Any change here needs
verification across all of them, not just Fox Hunt.

## Hypothesis B — Fox Hunt/Trailblazer's loop isn't "very tight" while keying

The classic `loop()` (`m32_v6.ino:945-975`) has an explicit, deliberate
pattern for this exact problem:

```cpp
case morseKeyer:  if (doPaddleIambic(leftKey, rightKey)) {
                      return;   // we are busy keying and so need a very tight loop !
                  }
```

— when the keyer is busy, `loop()` returns immediately with **no delay at
all**, so the Arduino framework re-enters it as fast as possible and
`doPaddleIambic()`'s own internal `millis()`-based element/gap timers get
serviced with minimal added jitter.

Fox Hunt's and Trailblazer's own loops (`statePlaying()` in both files) call
`delay(10)` **unconditionally at the end of every iteration**, regardless of
whether `doPaddleIambic()` just reported the keyer busy. Every extra ms
between two `doPaddleIambic()` calls while an element/gap timer is running is
extra ms added to whatever duration that timer was tracking — this directly
predicts a *lengthened* gap (and lengthened dit/dah, though that's much less
noticeable), matching row 2 above (the "too long" gap specifically observed
*in-game*, contrasted with the same paddle sounding right in plain Keyer
mode, which has no such delay).

**This is the more actionable, lower-risk half of the investigation** — it's
new code from this session (`MorseFoxHunt.cpp`, `MorseTrailblazer.cpp`), not
shared calibration, so fixing it (mirror the classic loop's pattern: skip or
shorten the delay while `doPaddleIambic()` reports busy) doesn't carry the
same cross-game regression risk as touching `MorseCwEngine`/`pwmTone`. Worth
doing on its own once confirmed, independent of Hypothesis A.

## Hypothesis C (lower confidence) — the mid-tone-cancel "still weird" residual

When Fox Hunt's `pollKeyedChar()` detects the keyer leaving idle and calls
`MorseCwEngine::playStop()` on an in-flight clue, that call's own
`pwmNoTone()` (if a tone was on) is the same blocking ~6ms call from
Hypothesis A — invoked at the exact moment the player's own keyer is
independently calling `pwmTone()`/`pwmNoTone()` for their own sidetone
through the same PWM hardware channels. Plausible source of the residual
"weird" quality even after the cancel-not-resume fix, but not measured or
confirmed — would need its own recording focused on that exact moment.

## Recommended next steps

1. **Controlled A/B recording** (same setup, same WPM, back-to-back): plain
   Keyer mode / Fox Hunt clue playback / Fox Hunt's own keying — needed
   before trusting any of the numeric magnitudes above.
2. **Hypothesis B fix** (Fox Hunt + Trailblazer loop tightening while
   keying) is the safer, more isolated one to try first — new code, no
   shared-engine blast radius.
3. **Hypothesis A** (`pwmTone`/`pwmNoTone` blocking ramp vs. `MorseCwEngine`'s
   scheduling) needs to stay hands-off until there's a controlled measurement
   and a plan that accounts for Pileup and Radio Cave too.
4. Re-measure Hypothesis C once A and B are better understood — it may
   simply disappear once the other two are addressed.
