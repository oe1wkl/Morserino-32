#!/usr/bin/env python3
"""Render the starter pack of replacement OK/ERR jingles as MP3s.

The firmware plays /sounds/success.mp3 and /sounds/error.mp3 in preference to its own
signals (soundSignalOK/soundSignalERR), and the Configuration Tool uploads them. So a
jingle pack needs no firmware change -- but it does need to respect two things:

  LEVEL. The manual's "-1 dBFS peak" is a CLIPPING CEILING, not a level target. A file
  normalised to -1 dBFS decodes to 0.891, and after SIDETONE_LEVEL and the DAC's +2 dB
  that is ~9 dB hotter in peak than the built-in OK signal -- which would walk straight
  back into the complaint the built-in tones were just fixed for. Every jingle here is
  therefore level-matched to the BUILT-IN pair (see TARGET_DB, measured as the loudest
  100 ms, A-weighted, through the speaker model), and lands far below the clipping
  ceiling as a side effect.

  DIRECTION. Every error jingle FALLS in pitch and every success jingle RISES. That is
  the M32's convention, on the classic since the beginning; a rising "error" is
  disorienting, and had crept into the Pocket once already.

Usage:  python3 make_jingles.py [outdir]
"""
import math, os, subprocess, sys
import numpy as np
from tone_sim import (SR, SIDETONE_LEVEL, DAC_GAIN, sidetone_ref, silence,
                      short_term_loudness_db, write_wav)

# vs the CW sidetone, i.e. where the built-in pair sits. Not equal: the acknowledgement
# is the more discreet of the two, because you get it far more often and it tells you
# nothing you did not already know (bench, 2026-09-05).
TARGET_DB = {"success": -9.0, "error": -5.5}
CEILING   = 10 ** (-1.0 / 20)   # -1 dBFS, the manual's clipping ceiling
SAFE_PEAK = 10 ** (-2.0 / 20)   # ...and where we actually sit, leaving a dB for the
                                # overshoot an MP3 decodes back with


def note(freq, dur_ms, partials, decay_s, attack_ms=4.0, detune=0.0):
    """One struck/blown note: partials as (ratio, amplitude) pairs, each decaying
    exponentially. Inharmonic ratios give bells and bars; integer ratios give
    instruments with a definite pitch."""
    n = int(dur_ms / 1000.0 * SR)
    t = np.arange(n) / SR
    y = np.zeros(n)
    for ratio, amp in partials:
        # higher partials die away faster -- true of every struck bar and bell, and
        # what stops the result sounding like a synthesiser
        d = decay_s / (1.0 + 0.6 * (ratio - 1.0))
        y += amp * np.exp(-t / d) * np.sin(2 * math.pi * freq * ratio * (1 + detune) * t)
    a = max(1, int(attack_ms / 1000.0 * SR))
    y[:a] *= np.linspace(0.0, 1.0, a) ** 0.5
    y[-64:] *= np.linspace(1.0, 0.0, 64)          # never end on a step
    return y


def seq(items, gap_ms=0.0):
    """Notes one after another, overlapping by their own decay if gap is negative."""
    out = np.zeros(0)
    for y in items:
        if gap_ms < 0 and len(out):
            ov = min(int(-gap_ms / 1000.0 * SR), len(out), len(y))
            head, tail = out[:-ov], out[-ov:] + y[:ov]
            out = np.concatenate([head, tail, y[ov:]])
        else:
            out = np.concatenate([out, silence(gap_ms), y]) if len(out) else y
    return out


# Partial recipes ------------------------------------------------------------------
BRASS   = [(1, .50), (2, 1.0), (3, .90), (4, .72), (5, .52), (6, .34), (7, .20), (8, .10)]
REED    = [(1, .25), (2, 1.0), (3, .85), (4, .45), (5, .18), (6, .07), (7, .03)]
BELL    = [(1, 1.0), (2.76, .55), (5.40, .28), (8.93, .14), (13.3, .06)]   # handbell
MARIMBA = [(1, 1.0), (3.9, .50), (9.2, .18)]                               # tuned bar
GLASS   = [(1, 1.0), (2.0, .45), (3.01, .22), (4.02, .10)]                 # soft chime
PULSE   = [(1, 1.0), (3, .33), (5, .20), (7, .14), (9, .11)]               # chiptune

# Notes (equal temperament)
def hz(n):  # MIDI number -> Hz
    return 440.0 * 2 ** ((n - 69) / 12.0)

C5, E5, G5, C6, D6, E6 = hz(72), hz(76), hz(79), hz(84), hz(86), hz(88)
A5, D5, F5, A4, F4     = hz(81), hz(74), hz(77), hz(69), hz(65)
D4, E4, G4, Bb4, B4    = hz(62), hz(64), hz(67), hz(70), hz(71)
Fs4, Ds4, Cs5, C4      = hz(66), hz(63), hz(73), hz(60)

# Every error jingle lives at or above ~290 Hz. Below that this speaker radiates
# almost nothing (see speaker_mag in tone_sim), so a low, "serious"-sounding error
# either disappears or has to be scaled past full scale to be heard -- which is how
# the built-in ERR ended up as a clipped square in the first place. Falling gestures
# in the 300-600 Hz region carry perfectly well and still read as negative.

# The pack -------------------------------------------------------------------------
JINGLES = {
    # A short fanfare, closest in spirit to the built-in pair -- for people who like
    # the built-ins but want a bit more of an occasion made of it.
    "fanfare": {
        "success": seq([note(C5, 110, BRASS, .30), note(E5, 110, BRASS, .30),
                        note(G5, 300, BRASS, .55)], gap_ms=-14),
        "error":   seq([note(A4, 150, REED, .34), note(F4, 150, REED, .34),
                        note(D4, 430, REED, .70)], gap_ms=-18),
    },
    # Handbells. Bright and clean; the inharmonic partials keep it from sounding
    # like a test tone.
    "bells": {
        "success": seq([note(A5, 260, BELL, .45), note(E6, 620, BELL, .80)], gap_ms=-90),
        "error":   seq([note(D5, 280, BELL, .55), note(A4, 700, BELL, .95)], gap_ms=-90),
    },
    # Wooden and dry. The shortest pair here -- good if you find any ringing tone
    # intrusive between repetitions.
    "marimba": {
        "success": seq([note(D5, 130, MARIMBA, .22), note(A5, 130, MARIMBA, .22),
                        note(D6, 300, MARIMBA, .34)], gap_ms=-30),
        "error":   seq([note(Cs5, 200, MARIMBA, .55), note(G4, 460, MARIMBA, .80)],
                       gap_ms=-60),
    },
    # Nearly pure, slow, quiet. For a quiet room, or for anyone who finds the
    # built-in signals startling at all.
    "chime": {
        "success": seq([note(F5, 300, GLASS, .55, attack_ms=25),
                        note(C6, 700, GLASS, .95, attack_ms=25)], gap_ms=-140),
        "error":   seq([note(D5, 340, GLASS, 1.0, attack_ms=25),
                        note(A4, 820, GLASS, 1.5, attack_ms=25)], gap_ms=-140),
    },
    # Chiptune, for the games. Odd harmonics on purpose here -- that IS the sound --
    # but at a sane level, which is what the V9 beta's signals were not.
    "arcade": {
        "success": seq([note(C5, 55, PULSE, .10), note(E5, 55, PULSE, .10),
                        note(G5, 55, PULSE, .10), note(C6, 190, PULSE, .28)]),
        "error":   seq([note(Fs4, 110, PULSE, .18), note(E4, 110, PULSE, .18),
                        note(D4, 300, PULSE, .40)]),
    },
}


def match_level(y, target_db, ref_db):
    """Scale so the loudest 100 ms sits target_db below the CW sidetone -- but never
    above the peak ceiling. Returns (audio, shortfall_db).

    A jingle that cannot reach the target from under the ceiling is not a bug in the
    scaling, it is a design that puts too little of its energy where the speaker can
    radiate it: too low, or too peaky (a struck bar is mostly transient). Raising its
    pitch or filling in its partials fixes it; scaling harder only clips."""
    cur = short_term_loudness_db(y * SIDETONE_LEVEL * DAC_GAIN)
    g = 10 ** ((ref_db + target_db - cur) / 20)
    peak = float(np.max(np.abs(y))) * g
    if peak > SAFE_PEAK:
        g *= SAFE_PEAK / peak
        return y * g, 20 * math.log10(SAFE_PEAK / peak)
    return y * g, 0.0


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "jingles"
    ref_db = short_term_loudness_db(sidetone_ref())
    print(f"{'jingle':22s} {'vs CW':>7s} {'peak':>7s} {'dBFS':>7s} {'ms':>5s}")
    problems = []
    for pack, pair in JINGLES.items():
        os.makedirs(os.path.join(out, pack), exist_ok=True)
        for kind, y in pair.items():
            y = y / max(np.max(np.abs(y)), 1e-9)          # normalise, then level-match
            y, shortfall = match_level(y, TARGET_DB[kind], ref_db)
            if shortfall < -6.0:
                problems.append(f"{pack}/{kind}: {shortfall:.1f} dB short of the target "
                                f"at the peak ceiling -- pitch it up or fill in its partials")

            wav = os.path.join(out, pack, f"{kind}.wav")
            mp3 = os.path.join(out, pack, f"{kind}.mp3")
            write_wav(wav, y)
            subprocess.run(["lame", "--quiet", "-b", "128", "-h", "--resample", "44.1",
                            wav, mp3], check=True)
            # An MP3 decodes back a little hotter than it went in -- that overshoot is
            # exactly why the manual specifies a ceiling. Measure it, don't assume it.
            # Decode stereo and take ONE channel. "-ac 1" is not a neutral downmix:
            # for identical L/R it sums rather than averages, and reads ~2.6 dB hot.
            dec = subprocess.run(["ffmpeg", "-v", "quiet", "-i", mp3, "-f", "f32le",
                                  "-"], capture_output=True, check=True)
            d = np.frombuffer(dec.stdout, dtype="<f4")[0::2]
            dpeak = float(np.max(np.abs(d))) if len(d) else 0.0
            dl = short_term_loudness_db(d * SIDETONE_LEVEL * DAC_GAIN) - ref_db
            if dpeak > CEILING:
                problems.append(f"{pack}/{kind}: decoded peak {dpeak:.3f} over the ceiling")
            os.remove(wav)
            print(f"{pack + '/' + kind:22s} {dl:+7.1f} {dpeak:7.3f} "
                  f"{20 * math.log10(max(dpeak, 1e-9)):7.1f} {len(y) / SR * 1000:5.0f}")
    print("\n" + ("PROBLEMS\n  " + "\n  ".join(problems) if problems
                  else f"OK - {len(JINGLES)} pairs, all level-matched to the built-in "
                       f"signals and clear of the -1 dBFS ceiling"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
