# Ultimatic and the Curtis-B timing machinery

**Status: fixed** on branch `ultimatic-memory-model` (Option 3 below, chosen by Willi).
Builds clean for both variants; **not yet bench-tested**.

Second report from **Paweł, SP5DNA** (September 2026), after bench-testing the
point-6 fix at 5 WPM on a Pocket. Two symptoms, both reproduced here exactly.
Neither has anything to do with `54ce495` — see finding 1 below.

## 1. What he found, and it all reproduces

Scenario for both: a **plain squeeze** — dah opens the character, dit is squeezed
in 30 ms later, both released together at `rel`. No re-closure anywhere, so the
point-6 logic is never consulted.

* **Bug A — a brief squeeze is swallowed.** Press and release the dit inside the
  first 45 % of the dah and it is lost completely: output is a bare dah where it
  should be dah + dit. At 5 WPM the tap is discarded for `rel` in **[20, 322] ms**.
* **Bug B — a recurring window that splices in an unwanted dah.** Once the dit has
  taken control, releasing both paddles inside a **178 ms window of each 475 ms
  cycle** produces one extra dah. Release earlier or later in the same cycle and the
  result is correct.

His millisecond boundaries are exact: the bug windows measure
`[1134-1311] [1609-1786] [2084-2261]`, all 178 ms, matching his report to the
millisecond. His arithmetic for *why* (`curtistimer` = 2 + 240×75/100 = 182,
plus the 120 ms `latencytimer`, = 178 ms) is also right, and his correction of his
own first draft — that `D` and `B` are correct continuations and not part of the
bug — is right too.

Reproduce with `./keyer_sim scan`.

## 2. Three findings that change the framing

**1. Both symptoms predate the point-6 fix.** Running the same scans through the
`LEGACY` logic gives byte-identical windows. `54ce495` neither caused nor masked
them; it is not implicated either way.

**2. Neither symptom is Ultimatic-specific.** Bug A's window is *identical* in
Iambic B ([20-322] ms), and *wider* in Iambic A (the whole element), because this
is the Curtis-B percentage gate doing exactly the job it was built for: the
opposite paddle is not sampled until `CurtisB DahT%`/`DitT%` of the element has
elapsed. Bug B's extra element is Curtis B's *defining feature* — release both
paddles during an element and mode B appends one more of the opposite type.

So the accurate statement is not "Ultimatic is implemented incorrectly" but
**"the M32's Ultimatic is an Iambic-B-flavoured Ultimatic"**, which is exactly the
diagnosis Paweł reached from the other end.

**3. That sharing is documented, in both languages.** The manual's preference
table says of `CurtisB DahT%` and `CurtisB DitT%`: *"Also influences the behavior
in Ultimatic mode!"*, and the CW Keyer section says Ultimatic *"responds to entries
activated on the opposite paddle with the same timing preferences defined for
Iambic B mode."* This is a deliberate, shipped design decision, not an oversight.

None of that makes him wrong — both behaviours *are* deviations from the 1955
points 1/4/7. It means the question is a design one, not a defect one.

## 3. It is partly tunable today, but not fully

The two symptoms turn out to be driven by *independent* knobs — A by the dah
percentage, B by the dit percentage and Latency. Per-cycle bug-B window at 5 WPM,
out of 475 ms:

| Setting | Bug A (tap lost) | Bug B window |
|---|---|---|
| **Shipped defaults** (DahT% 45, DitT% 75, Latency 4) | 303 ms | 178 ms (37 %) |
| DitT% → 0 | 303 ms | 353 ms (74 %) — **worse** |
| Latency → 0 | 303 ms | 59 ms (12 %) |
| CurtisB 0/0 + Latency 0 | gone | 234 ms (49 %) — **worse** |
| **DahT% 0, DitT% 75, Latency 0** | **gone** | **59 ms (12 %)** |

So `CurtisB DahT% = 0` + `Latency = 0` removes Bug A outright and cuts Bug B to a
third, with no code change at all. Note the trap in the middle rows: zeroing the
*dit* percentage, the intuitive "make it sample sooner" move, doubles Bug B.

No settings combination removes Bug B entirely — that needs code.

## 4. A note on the earlier review

The extra element was visible in the first round and I classified it as intended:
`FINDINGS.md` §5 says assertions are scoped to before the release because "letting
go inside the Curtis-B early-latch window legitimately produces one extra element,
which is real firmware behaviour". That was accurate as a description and wrong as
a stopping point — having seen it, the question "should Ultimatic have Curtis-B
extra elements at all?" should have gone to Willi then rather than being settled
silently. Paweł's criticism of the `T4` test is fair: releasing at 700 ms happens
to land in the correct part of the cycle, so it never probed the window.

## 5. Options considered

**Option 0/1 — leave it, document the tuning recipe.** Zero risk, but Bug B stays
reachable and Ultimatic stays a deviation from the original.

**Option 2 — bypass the timing gates in Ultimatic.** Smaller, but it would have
killed the Latency preference for the mode.

**Option 3 — give Ultimatic its own memory model. → chosen.**

## 6. What was implemented

Ultimatic no longer decides from the sampled `DIT_L`/`DAH_L` latches. Instead:

* **Memories are armed by a paddle *closure*** (`ultDitMemory` / `ultDahMemory`,
  set on the rising edge that `54ce495` already tracked) and **consumed when an
  element starts**. A closure therefore guarantees its element no matter how brief
  it was or where in the current element it fell — which is Bug A gone, and points
  1 and 7 honoured for the first time.
* **The decision uses the *live* paddle levels**, not the sticky latches, via a new
  `ultimaticSelect()` called at the end of `INTER_ELEMENT` for `ULTIMATIC` only. A
  latch left over from a paddle that has since been released can no longer key
  anything — which is Bug B gone.
* `ultimaticSelect()` writes its answer back into the latch bits, so the existing
  `switch (keyerControl)` — and all the end-of-character bookkeeping in its
  `case 0/4` — carries on unchanged. The priority order is §8 of SP5DNA's spec:
  both memories → last closure; one memory → that one; no memory and both held →
  last closure; one held → that one; nothing → the character ends.
* **`CurtisB DahT%` / `DitT%` no longer affect Ultimatic at all.** They are Iambic-B
  timing knobs and Ultimatic has no use for them. The manual said the opposite in
  both languages; that has been corrected.
* **Latency keeps a real job**, in a new role: a rising edge on the paddle *currently
  in control* is ignored inside the latency window, so contact chatter — or a
  momentary touch dropout — cannot arm a memory. This is what stops the edge-armed
  model from being twitchier than the old sampled one, and it happens to close the
  touch-dropout risk flagged in `FINDINGS.md` §6.

## 7. Verification

`./keyer_sim` and `./keyer_sim scan`, with the simulator mirroring the firmware:

| | legacy (pre-`54ce495`) | shipped (`54ce495`) | memory model |
|---|---|---|---|
| T1–T8c, the twelve QST scenarios | 3 violations | 0 | **0** |
| Bug A — tap lost | 303 ms of every dah | 303 ms | **none** |
| Bug B — unwanted dah | 178 ms of every 475 ms | 178 ms | **none** (also at Latency 7) |

Plus two checks on claims made when the option was chosen, rather than assumed:

* **Other modes are untouched.** Iambic A, Iambic B and Non-Squeeze produce
  byte-identical output to the shipped firmware across all 2581 release times of
  the squeeze scan.
* **Latency still does something.** With a 10 ms chatter on the controlling paddle:
  Latency 0 takes it as a real closure and adds an element; Latency 4 and 7 filter
  it out.

## 8. Still to do

**Bench-tested by SP5DNA**, about an hour on an M32 Pocket across a range of speeds
including 35–40 WPM, plus exercises and games: both reported symptoms gone, nothing
dropped, nothing extra. He then found the Latency problem below. Still owed: the
classic/OLED variant on hardware, and Iambic A/B/Non-Squeeze by ear to confirm the
simulator's "unchanged".

## 9. Round three — Latency was swallowing deliberate taps

Testing at 40 WPM, SP5DNA reported that Latency 0 % felt "close to flawless" while
87.5 % produced "artifacts appearing more often", and honestly flagged it as an
impression he could not separate from his own technique at that speed.

He was right, and it was not his technique. The first cut of this fix muted any
rising edge on the paddle in control for the whole Latency window. That window is
measured from the end of an element, and at 40 WPM (dit = 30 ms) it is long enough to
cover a genuine re-tap of the same paddle. Measured with two deliberate 12 ms taps
separated by a varying open interval (`./keyer_sim latency`):

| open interval between the two taps | Latency 0 | Latency 4 | Latency 7 |
|---|---|---|---|
| 6–14 ms | both sound | both sound | both sound |
| 18–26 ms | both sound | **one swallowed** | **one swallowed** |
| 30 ms | both sound | both sound | **one swallowed** |

So the higher the Latency, the more real taps disappeared — exactly the direction he
heard, and exactly why 0 % felt best to him.

The mistake was mechanism, not intent: a rising edge in a time window cannot
distinguish chatter from a deliberate re-tap. What distinguishes them is **how long
the paddle was open** — bounce is a couple of milliseconds, a deliberate release is
not. The guard is now that instead: a re-closure of the controlling paddle counts
unless it was open for less than `CHATTER_GUARD_MS` (5 ms). At 60 WPM a dit is 20 ms,
so no deliberate movement can fall inside it.

Result: deliberate taps survive at **every** Latency setting, bounce under 5 ms is
still filtered, and a 10–20 ms dropout is treated as the real release it is. Latency
consequently has **no effect at all** in Ultimatic — verified across all 2581 release
times of the squeeze scan at Latency 0, 4 and 7 — so the manual now says that of all
three Iambic timing preferences, in both languages.

The lesson for next time: "keep the preference working" was my own framing when the
options were put to Willi, and it drove a mechanism that was wrong. The preference
did not need saving; the behaviour did.
