# M32 UX Conventions

**The interaction grammar of the Morserino-32.** This covers *all* interactive
modes — the classic training modes (CW Keyer, CW Generator, Echo Trainer,
Koch Trainer, Transceiver modes, …), the games, and the QSO Bot. A user who
has learned one mode should be able to predict how the next one starts,
how speed and volume are changed, how it ends, and how to get back to the menu.

> **Status: DRAFT v0.2.** This document deliberately states conventions as
> rules even where current code diverges; the consistency audit will identify
> the divergences, and we then either fix the code or amend the rule.
> Items marked `TODO(audit)` need the audit's findings before they can be
> stated definitively.

---

## 0. Mode classes and what may differ

| Mode class | Hardware | Screen layout | Global mechanisms (§1–§2) |
|---|---|---|---|
| **Classic training modes** (Keyer, Generator, Echo Trainer, Koch Trainer, Transceiver, LoRa/WiFi modes, …) | both variants | shared top-bar + scroll-area layout | **mandatory** |
| **Games** (Morsel, Radio Cave, … `TODO(audit): complete list`) | M32 Pocket only (TFT) | free to exploit the TFT (graphics, maps, colors) | **mandatory** |
| **QSO Bot** | both variants (`CONFIG_QSO_BOT`) | follows classic layout | **mandatory** |

The principle: **screen layout may differ by mode class; control mechanisms
may not.** A game may look completely different from the Echo Trainer, but
changing the speed, adjusting the volume, and leaving the mode must work
identically in both. Whenever a game needs a mechanism that already exists in
the classic modes (speed change, volume, pause, exit), it must reuse that
mechanism — never invent a parallel one.

## 1. Controls — what the inputs always mean

`TODO(audit): verify the current de-facto semantics in the CLASSIC modes
first — they are the incumbent standard that has been in users' muscle memory
for years. Games and QSO Bot conform to the classic modes, not vice versa.`

| Input | Convention (to be confirmed against classic modes) |
|---|---|
| **Black (encoder) knob — turn** | In menus: navigate. In running modes: adjust the primary live parameter (speed — `TODO(audit): confirm`) |
| **Black knob — click** | Confirm / activate; in running modes `TODO(audit)` |
| **Black knob — double click** | `TODO(audit)` (volume mode? context-dependent? — must be the *same* everywhere) |
| **Black knob — long press** | Leave the current mode / return to the menu (universal "escape hatch") |
| **Red button** | `TODO(audit): document its universal meaning in classic modes (start/stop? pause?)` |
| **Paddle / key** | CW input where the mode accepts it; paddle activity counts as user activity (TOT reset) |

Rules:

- **One escape hatch, everywhere.** The same gesture exits every mode —
  classic, game, or bot — at any point.
- **No mode may redefine a global gesture.** If a mode needs an extra action,
  it uses an input unassigned in that context.
- Any input that constitutes "user activity" resets the timeout
  (`MorseOutput::resetTOT()`).

## 2. Global live adjustments — speed, volume, pitch

These are the mechanisms most likely to have drifted, and the most important
to unify, because users exercise them in every mode:

- **Speed (WPM):** changed the same way in every running mode
  (`TODO(audit): document the classic mechanism — encoder turn while running?`).
  The displayed WPM updates immediately, in the same top-bar position, in
  every mode that shows it. Modes that deliberately manage speed themselves
  (e.g. Morsel's ramp-down) still *display* speed the standard way and
  restore the user's setting on exit.
- **Volume:** one gesture to enter volume adjustment, identical everywhere
  (`TODO(audit): document the classic mechanism`). A mode may never trap or
  repurpose this gesture.
- **Pitch / tone:** set in preferences only, never per-mode
  (`TODO(audit): confirm`).
- **Entering the preferences menu:** one gesture, identical from every mode
  where it is permitted (`TODO(audit): document the classic mechanism — long
  press red? — and from which states it works`). If a mode cannot allow it
  mid-round (e.g. mid-game), it allows it from its start/result screen.
- Adjustments made while in a mode **persist** the same way everywhere
  (`TODO(audit): confirm what is saved immediately vs. on exit`).

## 3. Lifecycle of an interactive mode

1. **Entry** from the menu shows the mode's start state: mode name and the
   live parameters that matter (WPM, Koch level, …). Classic modes may start
   "armed" awaiting input, as they traditionally do
   (`TODO(audit): document per classic mode`); games always show an explicit
   **start screen** — nobody is dropped into a running game.
2. **Start** in games is an explicit user action
   (`TODO(audit): click or first CW input — pick one`).
3. **During operation,** top bar and content area follow §4 and §5.
4. **Interruption:** modes with resumable state (e.g. Radio Cave) save to NVS
   automatically on exit — the user is never asked "save? y/n" on the device.
5. **End of a game round** shows a **result screen**: outcome, score, and the
   two standard choices — *play again* / *back to menu* — mapped to the same
   inputs in every game. (Classic modes have no rounds; they run until exited.)
6. **Exit** always returns to the same place in the menu the user came from.

## 4. Top bar

- The top bar always shows the **mode identity** plus the most useful live
  values (typically WPM; in games possibly score or — Radio Cave — the mini
  map).
- **Shared slots:** values that exist in multiple modes (WPM above all)
  appear in the **same position and format** in every mode that shows them,
  classic or game. `TODO(audit): inventory current top bars across ALL modes
  and define the standard slots.`
- The top bar is the only chrome in classic modes; the scroll area below is
  reserved for content. Games may use the area below freely (TFT graphics),
  but keep the top bar conventions.

## 5. Text and scroll area (classic modes; games where they show text)

- Output uses the shared scroll area with `NoOfVisibleLines` visible lines;
  wrapping is done by `DisplayWrapper`, never by the display library.
- **Echo conventions:** `TODO(audit): standardize how user CW input vs.
  machine-sent CW vs. system messages are distinguished across Echo Trainer,
  Transceiver modes, QSO Bot, and games, and apply uniformly.`
- Prosigns are displayed in their conventional form
  (`TODO(audit): confirm current rendering, e.g. <ka>, <ar>, <sk>`) and are
  never broken across lines.

## 6. CW behavior

- All machine-generated CW respects the **user's configured speed**; where a
  mode varies speed deliberately (Echo Trainer adaptivity, Morsel ramp-down),
  the variation is relative to the user's setting, explained in the manual,
  and the setting is restored on exit.
- Training content respects the **Koch level** where character filtering is
  meaningful (`TODO(audit): list which modes filter and which intentionally
  don't, with the stated reason`).
- Sidetone pitch and volume always follow the global preferences; no mode
  sets its own tone parameters.
- QSO-style exchanges (QSO Bot, Radio Cave's final QSO, Transceiver practice)
  follow **real CW operating conventions** — correct prosigns, plausible
  exchange structure, standard abbreviations. When in doubt, ask Willi; he is
  the domain authority.

## 7. Scores, progress, and persistence

- High scores persist in **NVS** using the canonical key scheme (CLAUDE.md
  §4). One scheme for all games.
- The result screen always shows this round's result and the relevant high
  score, in the same layout across games.
- Resumable modes save on exit and offer **Resume / New game** on next entry —
  same wording, same input mapping, wherever it appears.
- `TODO(audit): decide and document a uniform way to reset scores.`

## 8. Timeouts and errors

- The global TOT applies in all modes; any user activity resets it.
- Per-round timeouts inside games are communicated before they first matter,
  and expiry produces a consistent result (timeout counts as a miss —
  `TODO(audit): confirm uniform rule`).
- Error/edge messages are short, English, one tone: factual, no exclamation
  marks.

## 9. Multiplayer (ESPNow / LoRa / WiFi)

`TODO(audit): document the common pairing/start flow. The classic Transceiver
modes (LoRa/WiFi) are the incumbent pattern — Morsel multiplayer and any
future multiplayer mode should feel like them where the concepts overlap:
how a session is initiated, how the peer is shown, what happens on disconnect.`

## 10. Settings

- Mode-specific settings are reached and edited the same way for every mode —
  via the same preferences mechanism the classic modes use
  (`TODO(audit): document it and check the games conform`).
- Settings changes take effect the next round at the latest, and persist.

## 11. Manual parity

Every convention here corresponds to a sentence in the user manual. When a
convention changes, both the English and German manuals change with it. The
section structure of this document is intended to map onto the manual's
chapters, so keep headings stable.
