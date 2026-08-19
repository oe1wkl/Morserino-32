# Ultimatic mode vs. the 1955 original

**Status:** bug confirmed and fixed on branch `ultimatic-conformance`.
Reported by **Paweł, SP5DNA** (August 2026), who went back to the original QST
article rather than to second-hand descriptions. His reading was correct.

---

## 1. The source

Ultimatic was published as **John Kaye, W6SRY, "The All-Electronic 'Ultimatic'
Keyer, Part II — How It Works", QST, May 1955**, p. 37 ff., continued on pp. 122
and 128. The normative part is the boxed list **"Summary of Actions of the
Keys"** (p. 122/128), seven numbered points.

The scan (`worldradiohistory.com`, QST-1955-05.pdf) has no text layer, so the
text below was OCR'd from the page images to verify SP5DNA's transcription.
It is faithful. The point at issue is **point 6**:

> 6) Release and reclosure of the first key (just a flick!) reactuates the first
> memory and seizes control of the sequencor — the second key closed all the
> while — and the output reverts to the first character type until that first
> key is again released or until the opposite type character is flicked in by
> the second key. At least one character of the first type is guaranteed by the
> memory.

and its generalisation, point 7:

> 7) In recapitulation, any closure of a key guarantees at least one character
> of that type, transmitted in correct relationship to the order of closure,
> regardless of intervening selective motions. Whenever a key makes contact, the
> output subsequent to the character in progress corresponds to that key until
> the other key makes contact or the first key is released.

The operative idea in both: **control belongs to the key that closed *last*.**
Not to the key that opened the character.

## 2. What the firmware did

`doPaddleIambic()` in `m32_v6.ino` decided the Ultimatic case from `DIT_FIRST`:

```cpp
case ULTIMATIC:   if (DIT_FIRST) setDAHstate(); else setDITstate();
```

i.e. *"send the opposite of whichever paddle opened this character"*.

`DIT_FIRST` is written in exactly one place — `IDLE_STATE`, when a character
begins — and `IDLE_STATE` is only re-entered once **both** paddles have been
released. So for the whole duration of a squeeze it is frozen. On top of that,
the paddle latches are **level**-sampled: `updatePaddleLatch()` only ORs a bit in
while a paddle reads closed, and nothing anywhere detected a *rising edge*.

A release and re-closure of a paddle was therefore literally invisible to the
state machine as long as the other paddle stayed down: the latch bit was already
set and stayed set, and the decision input (`DIT_FIRST`) could not change.

That is exactly point 6 not being implemented, and it is what SP5DNA described:
the common case (hold the first paddle, tap the second) works, the symmetric case
does not.

**Points 1–5 and 7 were already correct.** This was a single missing behaviour,
not a broken mode.

## 3. Scope — wider than "a flick"

The failure is not limited to a deliberate sub-100 ms flick. Because the trigger
is a *re-closure*, and the firmware saw no closures at all, the following also
produced nothing (scenario T8c in the simulator):

* dah opens the character, dit is squeezed in and held → control passes to dit;
* dah is released **for 450 ms**;
* dah is closed again, dit still held.

The 1955 keyer sends a dah. The Morserino sent an unbroken stream of dits — a
half-second, fully deliberate paddle movement with no effect whatsoever. That is
a good deal more likely to be met in ordinary sending than the flick itself.

## 4. The fix

Track the most recent paddle **closure** live, and let that decide:

```cpp
static boolean prevDit = false, prevDah = false;
if (dit && !prevDit)  DIT_LAST_CLOSED = true;
if (dah && !prevDah)  DIT_LAST_CLOSED = false;
prevDit = dit; prevDah = dah;
...
case ULTIMATIC:   if (DIT_LAST_CLOSED) setDITstate(); else setDAHstate();
```

The edge tracking sits at the top of `doPaddleIambic()`, right after the polarity
swap, so it runs on every call regardless of keyer state — the latch sampling
windows (Curtis-B early latch, and the Latency "deaf" window) deliberately look
at one paddle only, and must not be allowed to swallow an edge.

Notes on the design:

* **`DIT_FIRST` is kept, and still used by Non-Squeeze mode.** Non-squeeze
  *deliberately* ignores the second paddle, so it needs the static
  "which paddle opened the character" flag. Only the Ultimatic branch changed.
* **Simultaneous closure: the dah wins `DIT_LAST_CLOSED`**, while `IDLE_STATE`
  still starts the character with a dit. That combination reproduces the
  long-standing M32 behaviour for a plain squeeze — one dit, then a series of
  dahs — bit for bit (simulator scenario TSQ). It also happens to be the
  tie-break SP5DNA's own spec document recommends.
* **The latches did not need to become edge-armed.** The blind sampling windows
  only ever blind the paddle whose element was just sent; the *opposite* paddle
  is always sampled, and by the time the inter-element decision is taken
  (`ktimer`, a full dit after key-up) the latency window has always expired and
  both paddles have been sampled at least once. So a meaningful closure cannot
  be lost, and the Latency preference keeps working as documented.

Naming: `DIT_LAST_CLOSED` is *not* the existing `DIT_LAST` latch bit, which says
which **element** was sent last. The declaration carries a comment saying so.

## 5. Evidence — `keyer_sim.cpp`

`keyer_sim.cpp` in this directory is a host-side transcription of
`doPaddleIambic()`: same states, same latch bits, same timer arithmetic, same
order of the `INTER_ELEMENT` decision, and it also models the `loop()` quirk that
`checkPaddles()` is skipped while `keyerState` is `DIT`, `DAH` or `KEY_START`.
Display, LoRa and decoder side effects are dropped. It runs both the old and the
new selection logic over the same scripted paddle timelines and checks the result
against the seven QST points.

```
c++ -std=c++17 -O1 -o keyer_sim keyer_sim.cpp && ./keyer_sim
VERBOSE=1 ./keyer_sim        # adds per-element start/end timestamps
```

At 12 wpm (dit 100 ms, latency 4/8):

```
test  QST  scenario                                                    legacy/fixed  legacy    fixed
T1a   1    tap dit once -> exactly one dit                             ok   / ok     .         .
T1b   1    tap dah once -> exactly one dah                             ok   / ok     -         -
T2a   2    hold dit -> a run of dits                                   ok   / ok     ....      ....
T2b   2    hold dah -> a run of dahs                                   ok   / ok     ----      ----
T3    3    dah opens, dit squeezed in -> dah then dit (letter N)        ok   / ok     -.        -.
T4    3+4  dah held, dit squeezed and held -> one dah, then only dits   ok   / ok     -..       -..
T5    5    dit released with dah still closed -> reverts to dah         ok   / ok     -.---     -.---
T5b   1+7  two separate dit closures under a held dah -> two dits       ok   / ok     -..--     -..--
T8    6    dah opened it, dit held, dah FLICKED -> dah seizes back     FAIL / ok     -.....    -.--
T8b   6    mirror image: dit opened it, dah held, dit FLICKED          FAIL / ok     .----     .-.....
TSQ   3    both paddles closed in the same instant (regression)         ok   / ok     .----     .----
T8c   6+7  opening paddle released for good, then re-closed             FAIL / ok     -......   -...--
```

Assertions are scoped to the window before the paddles are released: letting go
inside the Curtis-B early-latch window legitimately produces one extra element,
which is real firmware behaviour and says nothing about Ultimatic selection.

Two things worth knowing if you extend the simulator:

* **T5b is not "two taps always give two dits".** The M32 latch, like the 1955
  memory, is a bistable, not a counter. Two closures that both land before the
  same decision yield **one** element. T5b spaces them so the first is consumed
  first. Both the old and the new code get this right.
* **After a flick, control *stays* with the flicked paddle** (a run of its
  element type), it does not alternate back after one element. Point 6 says
  "until that first key is again released or until the opposite type character
  is flicked in by the second key". An early draft of these tests asserted
  alternation and was wrong.

## 6. Still to do — on the bench

The change is verified by simulation and builds clean for both variants; it has
**not** been tried on hardware yet. Worth checking:

1. **Mechanical paddle on the 3.5 mm jack** — the primary case. Hold one paddle,
   squeeze and hold the other, then flick the first: an element of the first type
   must appear, and keep repeating until that paddle is released again.
2. **Capacitive touch paddles** — does a flick actually produce a clean release
   through `readSensors()` + the 0.512 ms debounce, or is the pad still "warm"?
   Also the converse: while both pads are held, a momentary *dropout* of one pad
   now injects one element of that type, where before it was harmless. If touch
   turns out to be twitchy here, the mitigation is a minimum-open time before a
   re-closure counts as an edge — not a revert.
3. **Ordinary sending unchanged** — plain squeezed characters (C, K, R, period,
   AR) should feel exactly as before; TSQ says they are identical, but confirm by
   ear.
4. Both paddle polarities, and the external-paddle polarity preference, since the
   edge tracking sits after the swap.
