#!/usr/bin/env python3
"""Render the OK/ERR listening tests. See README.md for what is in each file."""
import os, sys
import numpy as np
from tone_sim import *

OUT = sys.argv[1] if len(sys.argv) > 1 else "demos"
os.makedirs(OUT, exist_ok=True)

# Levels chosen so each signal lands ~6 dB BELOW the CW sidetone on the speaker model
# (today's OK lands 3 dB ABOVE it). Solved by tone_sim, rounded.
PROP = {
    "ok":  dict(f=OK_HZ_PREV9,  timbre="trumpet", level=0.43),   # 440/587, classic pitch
    "err": dict(f=ERR_HZ_PREV9, timbre="bassoon", level=0.61),   # 366/330, classic pitch
}
TODAY = {
    "ok":  dict(f=OK_HZ,  timbre="square4", level=0.71),
    "err": dict(f=ERR_HZ, timbre="square4", level=0.71),
}

def sig(d, scale=1.0, softness=DEFAULT_SOFTNESS_MS):
    return signal(d["f"], d["timbre"], d["level"] * scale, softness)

def group(d, gap=450):
    """CW reference, then OK, then ERR -- the comparison that matters is against CW."""
    return np.concatenate([sidetone_ref(), silence(gap),
                           sig(d["okd"]), silence(gap), sig(d["errd"]), silence(gap)])

def pair(today, prop):
    return dict(okd=today["ok"], errd=today["err"]), dict(okd=prop["ok"], errd=prop["err"])

t, p = pair(TODAY, PROP)

# 1 -- the headline A/B
write_wav(f"{OUT}/1_today_vs_proposal.wav", np.concatenate([
    group(t), silence(1200), group(p)]))

# 2 -- timbre alone: same pitch, same level, only the harmonic recipe changes
same_lvl = 0.50
tim = []
for name, (okt, errt) in [("today (square, odd harmonics only)", ("square4", "square4")),
                          ("proposed (trumpet / bassoon)",       ("trumpet", "bassoon")),
                          ("pre-V9 (pure sine)",                 ("sine", "sine"))]:
    tim += [signal(OK_HZ_PREV9, okt, same_lvl), silence(250),
            signal(ERR_HZ_PREV9, errt, same_lvl), silence(1000)]
write_wav(f"{OUT}/2_timbre_only.wav", np.concatenate(tim))

# 3 -- how far down? each step preceded by the CW reference
lad = []
for db in (0, -3, -6, -9):
    s = 10 ** (db / 20)
    lad += [sidetone_ref("R"), silence(350),
            sig(PROP["ok"], s), silence(200), sig(PROP["err"], s), silence(1100)]
write_wav(f"{OUT}/3_level_ladder.wav", np.concatenate(lad))

# 4 -- pitch: does the musical timbre still carry at the classic M32's pitches?
pit = []
for f_ok, f_err in ((OK_HZ, ERR_HZ), (OK_HZ_PREV9, ERR_HZ_PREV9)):
    pit += [signal(f_ok, "trumpet", 0.43), silence(250),
            signal(f_err, "bassoon", 0.61), silence(1000)]
write_wav(f"{OUT}/4_pitch_v9_vs_classic.wav", np.concatenate(pit))

# 5 -- file 1 through the micro-speaker model (approximate; see tone_sim.speaker_mag)
sim = apply_speaker_sim(np.concatenate([group(t), silence(1200), group(p)]))
write_wav(f"{OUT}/5_speaker_sim_today_vs_proposal.wav", sim, gain=0.9 / max(abs(sim).max(), 1e-9))

ref = sidetone_ref(); rs = loudness_db(ref); rf = loudness_db(ref, False)
print(f"{'':34s} {'speaker':>9s} {'headphone':>10s}   (dB vs the CW sidetone)")
for tag, d in (("today  OK", TODAY["ok"]), ("today  ERR", TODAY["err"]),
               ("propose OK", PROP["ok"]), ("propose ERR", PROP["err"])):
    s = sig(d)
    print(f"{tag:34s} {loudness_db(s)-rs:+9.1f} {loudness_db(s,False)-rf:+10.1f}"
          f"   peak {abs(s).max():.3f}")
print(f"\nwrote 5 files to {OUT}/")
