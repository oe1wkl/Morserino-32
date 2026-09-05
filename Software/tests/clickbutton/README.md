# ClickButton — host unit tests

Off-device tests for `Software/src/Version 6 and newer/ClickButton.cpp`, the
button decoder that every M32 control gesture goes through — the ENCODER button
(`modeButton`) and the red FN button (`volButton`), in the menu, in every
training mode, in the preferences, in the games and in the QSO Bot.

These compile the **same `ClickButton.cpp` the firmware builds**, against a tiny
Arduino shim (`arduino_pin_shim.h`, reached through the local `Arduino.h` stub)
that scripts `millis()` and `digitalRead()`. Only a C++11 compiler is needed; no
device, no PlatformIO.

```sh
make run     # build + run; exits non-zero on failure (CI-friendly)
make         # build only
make clean
```

## What these guard

A **long** click is reported while the button is still *held* — unlike a short
click, which is only reported once the release has settled. So when a caller
acts on a long click, the press it belongs to is not over. Callers act on it by
leaving whatever loop they were in, and the code they return to then starts
polling the same button. If `clicks` still shows that long press, one gesture is
obeyed twice.

That produced a run of user-visible faults, each fixed on its own before the
cause was addressed centrally:

| | symptom |
|---|---|
| #215 | long-press out of Preferences also left the mode underneath |
| #216 | long-press out of a mode climbed several menu levels; one long press skipped a level |
| — | Morsel's internal screens (mode select, role pick) can fall back two steps |
| — | `audioLevelAdjust()` breaks on any `volButton.clicks` without clearing, so a long press to leave it can drop straight back in |

Waiting does not clear it by itself: the caller's first `Update()` after a
release nobody observed restarts the debounce timer, which makes the long-click
branch skip itself while the release branch is not yet old enough to fire. The
stale value therefore survives until the **second** `Update()`, however long the
caller waited — `test_long_click_late_caller()` pins that down.

The fix is one-shot delivery: a long click is visible from the `Update()` that
detects it until the next `Update()`, and no longer. That is not a new rule —
it is what already happened whenever the button stayed held, because the next
`Update()` re-entered the long-click branch and computed `0 - 0`. The change
only extends the same self-clearing to the release window, where that branch
cannot re-enter. Every caller in this firmware reads `clicks` immediately after
its own `Update()`, which is exactly the window this preserves.

## Scope

Covered: single long click (delivered once, to its owner only — with the caller
resuming before the release, while still held, and long after), short click,
double click, a fresh gesture following a long press, and two long presses in a
row. Reverting `ClickButton.cpp` alone makes three of these fail.

Not covered: bounce waveforms (the shim drives clean edges), `activeHigh`
buttons (the M32 wires both buttons active-low), and anything above the decoder
— whether a given screen *should* act on a gesture is not this layer's business.

## Note on the per-site `clicks = 0` swallows

Roughly a dozen call sites across `MorseMenu.cpp`, `MorsePreferences.cpp`, the
games and the QSO Bot clear `clicks` by hand. With the central fix they are no
longer load-bearing for the stale-long-click case, but they have **not** been
removed: several of them also discard a *legitimate* pending click, so that the
short press which chose a menu entry does not then get acted on by the screen it
opened. Telling the two purposes apart is a per-site judgement and worth doing
on its own, not folded into this change.
