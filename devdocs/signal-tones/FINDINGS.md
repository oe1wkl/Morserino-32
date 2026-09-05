# OK / ERR signalling tones — too loud, and too harsh

Field report (M32 Pocket, V9 beta):

> I notice the error / confirm tone has gotten very harsh and unpleasant — is there a way
> to lower the volume of the error / confirm tone? It seems like it is louder than the set
> volume. […] almost makes the games and echo trainer unenjoyable.

Both halves are real, both are ours, and both come from one commit: `35ab5c7`
*"Give the OK/ERR signals a richer timbre, and turn ERR the right way round"* (2026-08-30).
That commit was itself a fix for the opposite complaint — after PR #208 de-clipped the
codec, the signals had become far too quiet — and it over-corrected.

Evidence below is from `tone_sim.py`, a host-side renderer that mirrors the firmware's
audio chain sample for sample (`ComplexRotorSine` → `BlackmanHarrisEnvelope` →
`VolumeStream × SIDETONE_LEVEL` → DAC +2 dB). Sanity check on the model: it reproduces
the firmware's own bench figure — the CW sidetone peaks at 0.995 at the DAC against the
documented 0.9945.

## 1. Loudness — the signals really are louder than the CW

A-weighted level through a model of the Pocket's micro-speaker, relative to the CW
sidetone **at the same volume setting** (0.0 = exactly as loud as your CW):

| | speaker | headphones |
|---|---|---|
| pre-V9 OK — sine 440/587 | −3.9 dB | −1.1 dB |
| pre-V9 ERR — sine 366/330 | −17.0 dB | −4.8 dB |
| **shipping OK — 660/880** | **+3.0 dB** | −0.5 dB |
| **shipping ERR — 549/495** | −1.5 dB | −2.5 dB |

So the reporter is right, and precisely so: the confirmation tone is now *louder than the
CW you are practising*. The `-17.0` on the pre-V9 ERR row is the original bug that
`35ab5c7` set out to fix — the fix was needed, it just went 15 dB too far.

Two secondary findings in the same table:

- **OK is 4.5 dB louder than ERR** on the speaker, though both are generated at an
  identical digital level. 660/880 Hz with odd harmonics sits higher in the driver's
  efficient band than 549/495 Hz does. That is why the report names the *confirm* tone
  first.
- The problem is **speaker-specific**: on headphones the same signals sit sensibly below
  the sidetone. Anyone testing on phones will not reproduce it.

## 2. Harshness — the timbre is the least musical recipe available

`ComplexRotorSine::Timbre::Rich` is harmonics 1, 3, 5, 7 at 1, ⅓, ⅕, ⅐ — the first four
terms of a **square wave**. Odd harmonics only, rolling off at just 6 dB/octave, truncated
mid-series. That is the textbook hollow, buzzy, smoke-alarm timbre; the missing even
harmonics are exactly what makes it read as "electronic" rather than "musical". Both
signals use it, so OK and ERR differ only in pitch and direction.

## 3. Both are fixable in the same place, and the fixes help each other

The oscillator already sums phase-locked harmonics with per-harmonic weights. Making it
musical is a different **weight table**, not new machinery — 8 rotors instead of 4, under
1 % more CPU on the S3's FPU:

```
trumpet (OK)   h1..h8  0.50 1.00 0.90 0.72 0.52 0.34 0.20 0.10   full series, formant at h2-h3
bassoon (ERR)  h1..h8  0.25 1.00 0.85 0.45 0.18 0.07 0.03 0.01   weak fundamental, dark
```

The bassoon's weak fundamental is a **feature** on this driver: the ear still hears the
written pitch (missing fundamental) while the energy sits at 1–2 kHz, where the speaker
actually radiates. Musical voicing and audibility pull the same way here, not against
each other.

And because the harmonics now carry the tone, the **pitches can come back down** to the
classic M32's 440/587 and 366/330. That costs only 1–3 dB versus the fifth-up
transposition `35ab5c7` introduced, and restores the pitch identity the classic has always
had. The fifth up was only ever a workaround for a thin sine.

Proposed, landing both signals ~6 dB below the CW sidetone and — unlike today — **equal to
each other**:

| | speaker | headphones | digital peak |
|---|---|---|---|
| OK — trumpet 440/587, level 0.43 | −5.8 dB | −10.7 dB | 0.43 |
| ERR — bassoon 366/330, level 0.61 | −5.9 dB | −7.8 dB | 0.61 |

## 4. Where the level trim has to go

Not where you would expect. On the Pocket (`CONFIG_TLV320AIC3100`) the `volume` argument
of `MorseOutput::pwmTone()` is **dead** — that branch calls `sidetone.setVolume(SIDETONE_LEVEL)`
and ignores it; the audible level comes from the codec's analog volume, set once from
`sidetoneVolume`. So `soundSignalOK()` passing `MorsePreferences::sidetoneVolume` has no
effect at all today, and there is currently no way to trim these tones from the call site.

Three candidate trim points, only one of which is safe:

- ~~`soundSetVolume()` around the signal~~ (what `pwmClick` does) — two I²C writes per beep,
  and it disturbs shared analog volume state.
- ~~`sidetone.setVolume()`~~ — that `VolumeStream` is **shared with the accessibility voice
  clips**. Ducking it for a beep would duck any speech that overlapped.
- ✅ **a per-timbre level constant inside `ComplexRotorSine`** — purely digital, instant, no
  shared state, and it is where `kRichLevel` already lives.

## 5. Not affected

The classic M32 (OLED, PWM) is untouched by all of this: its sidetone *and* its signals are
both PWM square waves through the same `vol[]` table, so they track each other, and
`35ab5c7` deliberately left that path alone. The report mentions the games, which are
Pocket-only, and the numbers above are Pocket numbers.

## Listening tests

`python3 render_demos.py demos/` writes five WAVs:

| file | what to listen for |
|---|---|
| `1_today_vs_proposal.wav` | headline A/B. Each half: CW `CQ`, then OK, then ERR |
| `2_timbre_only.wav` | same pitch, same level, three timbres: today's square → trumpet/bassoon → pre-V9 pure sine |
| `3_level_ladder.wav` | the proposal at 0 / −3 / −6 / −9 dB, each after a CW `R` for reference |
| `4_pitch_v9_vs_classic.wav` | V9's fifth-up pitches vs the classic M32's, both musical |
| `5_speaker_sim_today_vs_proposal.wav` | file 1 through the micro-speaker model |

**Caveat.** A laptop reproduces 300–900 Hz perfectly and the Pocket's micro-speaker does
not. These files settle *timbre* and *relative level*; absolute loudness on the device
still needs a bench check. File 5 is an approximation, not a measurement.

---

# What shipped

Decided by Willi, 2026-09-05, from the listening tests above: **−6 dB, classic pitches,
trumpet/bassoon as rendered, fixed level (no preference).**

| | timbre | pitch | level | vs CW (speaker) | vs CW (phones) |
|---|---|---|---|---|---|
| OK | trumpet | 440 / 587 | 0.299 | −9.0 dB | −13.9 dB |
| ERR | bassoon | 366 / 330 | 0.61 | −5.9 dB | −7.8 dB |

**Bench-corrected 2026-09-05.** Both were first set to −6 dB and matched to each other.
On the device ERR was right and OK was still a little loud, so OK went to −9. The two are
now deliberately unequal, and that is the better design: you get the acknowledgement far
more often than the error, and it tells you nothing you did not already know, so it should
be the more discreet of the pair. The model ranked the options correctly but could not
have told us this — it is exactly the kind of judgement the bench is for.

Three files:

- `ComplexRotorSine.hpp` — 8 rotors instead of 4; `Timbre` is now
  `{ Sine, Trumpet, Bassoon }` with a weight table and a level constant per voice. The
  weights are looked up per sample from a static table rather than copied into the object,
  so `timbre_` stays a single-word write — the audio task reads it on **every** sample, not
  only while a tone sounds, so multi-word voice state could be torn.
- `I2S_Sidetone` — `setRichTimbre(bool)` → `setTimbre(ComplexRotorSine::Timbre)`.
- `MorseOutput.cpp` — `RichTone` → `SignalTone`, which takes the voice; OK and ERR back at
  the classic M32's pitches.

No preference was added, so **nothing is owed for accessibility** (§8): no new menu entry,
option value or on-screen message. Confirmed — re-running the extractor leaves
`voice_strings.txt` and `voice_clips.h` unchanged.

Builds clean for `pocketwroom`, `heltec_wifi_lora_32_V2` and `pocketwroom-accessibility`.

**Bench-checked on the device** (Willi, 2026-09-05): ERR confirmed good at −6 dB, OK taken
down to −9. Everything else above is modelled; the model was good enough to catch the bug
and rank the options, but the final level was settled by ear, as it had to be.

## Regression guard

`verify_firmware.py` reads the weights, the level constants and the pitches back **out of
the firmware sources** and re-measures them, failing if a voice drifts from the reference
recipe, if a level would clip, if either signal misses its own target against the CW
sidetone by more than 2 dB, or if the acknowledgement ever creeps up to within 1 dB of the
error signal. Run it after touching
`ComplexRotorSine.hpp` or `soundSignalOK`/`soundSignalERR`:

```
python3 verify_firmware.py [out.wav]
```

The relationship between the two is what is worth guarding, not their absolute levels: in
the V9 beta they were generated at an identical digital level and came out 4.5 dB apart --
the wrong way round -- because pitch decides how much of the energy lands in the driver's
efficient band. A code review cannot see that, and on the bench it presents as a vague
"the confirm tone is the annoying one".

## Follow-up: an MP3 jingle pack (Willi's idea, not yet built)

Both signals already fall back to user MP3s — `soundSignalOK()`/`soundSignalERR()` try
`/sounds/success.mp3` and `/sounds/error.mp3` first, and the Configuration Tool can upload
them. The manual documents this in the Echo Trainer section, including the −1 dBFS peak
guidance (a file normalised to 0 dBFS distorts, because the MP3 decoder runs out of
headroom before the codec does).

So a downloadable set of jingles needs no firmware work at all — it is a content and
packaging job: render a handful of pairs, hold them to that peak limit, and publish them
with a pointer from the manual. Worth keeping the built-in pair as the reference point for
level, so a jingle does not reintroduce the complaint this document is about.
