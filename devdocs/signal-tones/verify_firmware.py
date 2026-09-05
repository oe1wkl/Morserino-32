#!/usr/bin/env python3
"""Read the timbre constants back OUT of the firmware sources and re-measure them.

Guards against the two ways these numbers rot: someone edits a weight without
re-deriving the peak (-> the codec clips), or edits a pitch/level without re-checking
the balance against the CW sidetone (-> the V9 beta bug, where the confirmation beep
came out louder than the CW being practised).

Run after any change to ComplexRotorSine.hpp or soundSignalOK/ERR.
"""
import math, os, re, sys
import numpy as np
from tone_sim import (TIMBRES, peak_of, signal, sidetone_ref, loudness_db,
                      write_wav, silence, SR)

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
HPP = os.path.join(ROOT, "Software/src/vendor/cw-i2s-sidetone/include/ComplexRotorSine.hpp")
CPP = os.path.join(ROOT, "Software/src/Version 6 and newer/MorseOutput.cpp")

hpp, cpp = open(HPP).read(), open(CPP).read()
fail = []

def weights(name):
    m = re.search(r"k%s\[kHarmonics\]\s*=\s*\{([^}]*)\}" % name, hpp, re.S)
    return [float(x) for x in re.findall(r"-?[\d.]+(?=f)", m.group(1))]

def scale(name):
    m = re.search(r"return\s*\{\s*k%s,\s*([\d.]+)f\s*\}" % name, hpp)
    return float(m.group(1))

def pitches(fn):
    body = re.search(r"void MorseOutput::soundSignal%s\(\).*?\n}" % fn, cpp, re.S).group(0)
    i2s = body.split("#else", 1)[1]                     # the CONFIG_SOUND_I2S branch
    return [int(x) for x in re.findall(r"pwmTone\((\d+),", i2s)]

voices = {"Trumpet": ("trumpet", "OK"), "Bassoon": ("bassoon", "ERR")}
ref = sidetone_ref()
rs, rf = loudness_db(ref), loudness_db(ref, False)
demo = []

print(f"{'':38s} {'speaker':>9s} {'headphone':>10s} {'peak':>7s}")
for cname, (simname, fn) in voices.items():
    w, sc, p = weights(cname), scale(cname), pitches(fn)
    if w != TIMBRES[simname]:
        fail.append(f"{cname}: weights in firmware {w} != tone_sim {TIMBRES[simname]}")
    true_peak = peak_of(w)
    level = sc * true_peak
    if level > 0.95:
        fail.append(f"{cname}: level {level:.3f} risks clipping (peak x SIDETONE_LEVEL x DAC)")
    # the firmware's scale is (target level)/(true peak); reproduce that exactly
    sig = signal((float(p[0]), float(p[1])), simname, level)
    d_spk, d_hp = loudness_db(sig) - rs, loudness_db(sig, False) - rf
    print(f"{fn:4s} {cname:10s} {p[0]:>4}/{p[1]:<4} lvl {level:.3f} "
          f"{d_spk:+9.1f} {d_hp:+10.1f} {abs(sig).max():7.3f}")
    if not (-8.0 < d_spk < -4.0):
        fail.append(f"{fn}: {d_spk:+.1f} dB vs CW on the speaker, want -6 +/-2")
    demo += [sidetone_ref("R"), silence(350), sig, silence(900)]
    globals()[fn] = d_spk

if abs(OK - ERR) > 1.5:
    fail.append(f"OK and ERR differ by {abs(OK-ERR):.1f} dB; they should match within ~1 dB")

if len(sys.argv) > 1:
    write_wav(sys.argv[1], np.concatenate(demo))
    print(f"\nwrote {sys.argv[1]}")

print("\n" + ("FAIL\n  " + "\n  ".join(fail) if fail else "OK - firmware constants verified"))
sys.exit(1 if fail else 0)
