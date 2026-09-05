#!/usr/bin/env python3
"""
Host-side renderer for the M32 Pocket's OK / ERR signalling tones.

Mirrors the firmware's audio chain exactly, so candidate timbres and levels can be
auditioned and measured on a laptop before anything is flashed:

    ComplexRotorSine (harmonic sum, phase-locked, peak-normalised)
      -> BlackmanHarrisEnvelope (attack/release, "Tone Softness" pref, default 5 ms)
      -> VolumeStream  x SIDETONE_LEVEL (0.79)
      -> TLV320AIC3100 DAC  +2 dB              (PR #208)
      -> analog volume (speaker or headphone)  -- NOT modelled; it scales everything
                                                  equally and cancels out of every
                                                  comparison here.

Caveat: a laptop reproduces 300-900 Hz perfectly and the Pocket's micro-speaker does
not. Timbre and *relative* level are judgeable from these files; absolute loudness on
the device is not. --speakersim renders an approximation of the micro-speaker for that.

Usage:  python3 tone_sim.py [outdir]
"""

import math, os, struct, sys
import numpy as np

SR = 44100                     # sidetone.begin(44100, 16, 2, 128)
SIDETONE_LEVEL = 0.79          # MorseOutput.cpp
DAC_GAIN = 10 ** (2.0 / 20)    # codec DAC +2 dB, PR #208
DEFAULT_SOFTNESS_MS = 5.0      # "Tone Softness" pref default (value 4 -> 5 ms)

# ---------------------------------------------------------------------------
# Timbres: amplitude of harmonic n, n = 1..len(w). All summed as SINES, in phase,
# exactly as ComplexRotorSine::readSample() does for Timbre::Rich.
# ---------------------------------------------------------------------------
TIMBRES = {
    # Pure sine -- what OK/ERR sounded like before V9 (and what the CW sidetone is).
    # ComplexRotorSine uses the *real* part here, i.e. a cosine; irrelevant once the
    # envelope is applied, kept for fidelity.
    "sine":    [1.0],
    # SHIPPING TODAY: first four terms of a square wave, odd harmonics only at 1/n.
    # Odd-only 1/n is the classic hollow/buzzy recipe -- this is the harshness.
    "square4": [1.0, 0.0, 1/3, 0.0, 1/5, 0.0, 1/7],
    # Brass: full series, even AND odd, formant around the 2nd-3rd harmonic, smooth
    # roll-off. Bright and "full" rather than hollow.
    "trumpet": [0.50, 1.00, 0.90, 0.72, 0.52, 0.34, 0.20, 0.10],
    # Double reed: weak fundamental, strong 2nd-4th, hard roll-off above. Dark and
    # woody. The weak fundamental is a feature on this speaker: the ear still hears
    # the pitch (missing fundamental) while the energy sits at 1-2 kHz, where the
    # micro-speaker actually radiates.
    "bassoon": [0.25, 1.00, 0.85, 0.45, 0.18, 0.07, 0.03, 0.01],
}


def peak_of(weights):
    """True peak of the summed waveform -- the firmware's kRichPeak, per timbre."""
    t = np.linspace(0, 2 * math.pi, 200000, endpoint=False)
    y = np.zeros_like(t)
    for n, a in enumerate(weights, start=1):
        if a:
            y += a * np.sin(n * t)
    return float(np.max(np.abs(y)))


def bh_window(size, k):
    a0, a1, a2, a3 = 0.35875, 0.48829, 0.14128, 0.01168
    arg = k * 2.0 * math.pi / (size - 1)
    return a0 - a1*np.cos(arg) + a2*np.cos(2*arg) - a3*np.cos(3*arg)


def envelope(n_samples, softness_ms):
    """BlackmanHarrisEnvelope: rise = first half of a BH window, fall = 1 - rise."""
    n = max(1, int(softness_ms / 1000.0 * SR))
    if n % 2 == 0:
        n += 1
    n = min(n, 961)                                   # kMaxSamples
    k = np.arange(n)
    rise = bh_window(2 * n - 1, k)
    env = np.ones(n_samples)
    m = min(n, n_samples)
    env[:m] = rise[:m]
    env[-m:] = (1.0 - rise)[:m]          # release_table_, played in order from key-off
    return env


def tone(freq, ms, timbre, level, softness_ms=DEFAULT_SOFTNESS_MS):
    """One keyed tone. `level` is the firmware's kRichLevel: peak as a fraction of
    full scale AFTER peak-normalising the harmonic sum (so every timbre at the same
    `level` has the same peak, and the ear hears the difference in energy)."""
    w = TIMBRES[timbre]
    n = int(ms / 1000.0 * SR)
    t = np.arange(n) / SR
    y = np.zeros(n)
    if timbre == "sine":
        y = np.cos(2 * math.pi * freq * t)            # rotor_[0].re
    else:
        for h, a in enumerate(w, start=1):
            if a:
                y += a * np.sin(2 * math.pi * freq * h * t)
        y /= peak_of(w)
    y *= level
    y *= envelope(n, softness_ms)
    return y * SIDETONE_LEVEL * DAC_GAIN


def silence(ms):
    return np.zeros(int(ms / 1000.0 * SR))


# ---------------------------------------------------------------------------
# The two signals, with the firmware's exact timing (MorseOutput::soundSignalOK/ERR
# plus the delay(6) inside pwmTone and pwmNoTone).
# ---------------------------------------------------------------------------
OK_HZ, ERR_HZ = (660.0, 880.0), (549.0, 495.0)
OK_HZ_PREV9, ERR_HZ_PREV9 = (440.0, 587.0), (366.0, 330.0)


def signal(f_lo_hi, timbre, level, softness_ms=DEFAULT_SOFTNESS_MS):
    f1, f2 = f_lo_hi
    return np.concatenate([
        tone(f1, 6 + 97 + 6, timbre, level, softness_ms),
        silence(20),
        tone(f2, 6 + 193 + 6, timbre, level, softness_ms),
    ])


MORSE = {"C": "-.-.", "Q": "--.-", "R": ".-.", "T": "-", "V": "...-", "W": ".--"}


def sidetone_ref(text="CQ", freq=700.0, wpm=20):
    """A little real CW at the plain sidetone level -- the thing the OK/ERR signals are
    'louder than the set volume' compared to. Pure sine at full SIDETONE_LEVEL, i.e.
    exactly what the firmware emits for a keyed character."""
    dit = 1200.0 / wpm
    out = []
    for i, ch in enumerate(text):
        if i:
            out.append(silence(3 * dit))
        for j, el in enumerate(MORSE[ch]):
            if j:
                out.append(silence(dit))
            out.append(tone(freq, (3 if el == "-" else 1) * dit, "sine", 1.0))
    return np.concatenate(out)


# ---------------------------------------------------------------------------
# Measurement. Done in the frequency domain against the analytic transfer
# functions -- a cascade of hand-rolled biquads got this wrong by ~20 dB on the
# first attempt, because a 2nd-order section where the standard wants a 1st-order
# pole is invisible in the code and enormous in the answer.
# ---------------------------------------------------------------------------
def a_weight_mag(f):
    """IEC 61672 A-weighting, linear magnitude (unity at 1 kHz)."""
    f = np.maximum(f, 1e-6)
    f2 = f * f
    num = (12194.0 ** 2) * f2 * f2
    den = ((f2 + 20.6 ** 2)
           * np.sqrt((f2 + 107.7 ** 2) * (f2 + 737.9 ** 2))
           * (f2 + 12194.0 ** 2))
    return (num / den) * 10 ** (2.0 / 20)


def speaker_mag(f, f0=800.0, q=0.9, bump_db=6.0, bump_f=2800.0, bump_q=0.8):
    """Crude model of the Pocket's micro-speaker: a 2nd-order high-pass at its own
    resonance (nothing useful below ~800 Hz, 12 dB/oct), plus the broad 2-4 kHz
    presence region these drivers radiate best. Approximate, and deliberately so --
    it exists to keep laptop listening honest, not to specify the driver."""
    f = np.maximum(f, 1e-6)
    f2 = f * f
    hp = f2 / np.sqrt((f0 * f0 - f2) ** 2 + (f0 * f / q) ** 2)
    bump = 1.0 + (10 ** (bump_db / 20) - 1.0) / (1.0 + ((f / bump_f - bump_f / f) * bump_q * 3) ** 2)
    return hp * bump


def _weighted_rms(x, mag_fn):
    n = len(x)
    if n == 0:
        return 1e-12
    X = np.fft.rfft(x * np.hanning(n))
    f = np.fft.rfftfreq(n, 1.0 / SR)
    Y = X * mag_fn(f)
    # Parseval; the window cancels in every ratio we take.
    return math.sqrt(float(np.sum(np.abs(Y) ** 2)) / (n * n)) * math.sqrt(2)


def loudness_db(x, through_speaker=True):
    """A-weighted level of the sounding part, optionally through the speaker model.
    Only ever meaningful as a difference between two of these."""
    sounding = x[np.abs(x) > 1e-4]
    if len(sounding) == 0:
        return -120.0
    fn = (lambda f: a_weight_mag(f) * speaker_mag(f)) if through_speaker else a_weight_mag
    return 20 * math.log10(max(_weighted_rms(sounding, fn), 1e-12))


def apply_speaker_sim(x):
    """Render (not measure) through the speaker model, for listening on a laptop."""
    n = len(x)
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(n, 1.0 / SR)
    return np.fft.irfft(X * speaker_mag(f), n)


def write_wav(path, mono, gain=1.0):
    d = np.clip(mono*gain, -1.0, 1.0)
    pcm = (d*32767).astype('<i2')
    stereo = np.repeat(pcm, 2).tobytes()
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36+len(stereo)) + b'WAVEfmt ')
        f.write(struct.pack('<IHHIIHH', 16, 1, 2, SR, SR*4, 4, 16))
        f.write(b'data' + struct.pack('<I', len(stereo)) + stereo)
