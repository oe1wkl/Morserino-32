# M32 UX Conventions

**The interaction grammar of the Morserino-32.** This covers *all* interactive
modes — the classic training modes (CW Keyer, CW Generator, Echo Trainer,
Koch Trainer, Transceiver modes, …), the games, and the QSO Bot. A user who
has learned one mode should be able to predict how the next one starts,
how speed and volume are changed, how it ends, and how to get back to the menu.

> **Status: v0.3.** The `TODO(audit)` markers were resolved by the 2026-06
> consistency audit and folded in. Where current code still diverges from a
> rule, the rule states the **target** and points to `devdocs/divergences.md`
> for the gap and its fix; supporting evidence is in
> `devdocs/todo-resolutions.md`. Confirmed mechanics are stated as fact.

---

## 0. Mode classes and what may differ

| Mode class | Hardware | Screen layout | Global mechanisms (§1–§2) |
|---|---|---|---|
| **Classic training modes** (Keyer, Generator, Echo Trainer, Koch Trainer, Transceiver, LoRa/WiFi modes, …) | both variants | shared top-bar + scroll-area layout | **mandatory** |
| **Games** (Morse Invaders, Fight the Pileup, Radio Cave, Morsel) | M32 Pocket only (TFT) | free to exploit the TFT (graphics, maps, colors) | **mandatory** |
| **QSO Bot** (SOTA/POTA, Standard, Contest) | both variants (`CONFIG_QSO_BOT`) | follows classic layout | **mandatory** |

The principle: **screen layout may differ by mode class; control mechanisms
may not.** A game may look completely different from the Echo Trainer, but
changing the speed, adjusting the volume, and leaving the mode must work
identically in both. Whenever a game needs a mechanism that already exists in
the classic modes (speed change, volume, pause, exit), it must reuse that
mechanism — never invent a parallel one. The QSO Bot already does this
exemplarily; the games do not yet (§12, `divergences.md` H1–H3).

## 1. Controls — what the inputs always mean

These are the **confirmed de-facto semantics of the classic modes** (the global
`loop()` state machine in `m32_v6.ino`). They are the incumbent standard; games
and QSO Bot conform to them, not vice versa.

| Input | Meaning in classic running modes |
|---|---|
| **Black (encoder) knob — turn** | In menus: navigate. In running modes: adjust the parameter the encoder currently owns — **speed by default**, or volume / scroll-back / keyer-memory after a red-button gesture selects that role. |
| **Black knob — click (single)** | **Context action:** start/stop in Generator & Echo Trainer; select a keyer memory in Keyer & CW-Trx; confirm a selection in pickers. *(Allowed context split — see §1a.)* |
| **Black knob — double click** | **Enter the preferences menu.** (Not volume.) |
| **Black knob — long press** | **Leave the current mode → top menu.** The universal escape hatch; works in every mode at any point. |
| **Red button — single click** | **Toggle the encoder between speed and volume**; also exits scroll-back mode. |
| **Red button — long press** | **Scroll-back mode** (review scroll history with the encoder). |
| **Red button — double click** | Step **screen brightness**. |
| **Paddle / key** | CW input where the mode accepts it; paddle activity counts as user activity (TOT reset). |

Rules:

- **One escape hatch, everywhere.** Black-knob long-press exits every mode —
  classic, game, or bot — at any point. This is honored universally today.
- **No mode may redefine a global gesture.** If a mode needs an extra action, it
  uses an input unassigned in that context. (Today some games overload the
  red-button long-press — that is a divergence, `divergences.md` H2, not a
  licensed exception.)
- Any input that constitutes "user activity" resets the timeout
  (`MorseOutput::resetTOT()`). Classic modes reset it implicitly via screen
  updates; the documented pattern for new code is to call `resetTOT()`
  explicitly on each input, as the games do.

### 1a. Allowed context split — black-knob single click *(intentional)*

The single black-knob click deliberately means **start/stop** in Generator/Echo
Trainer but **select keyer memory** in Keyer/CW-Trx. These modes have no
conflicting need, the meaning is stable *within* each mode, and the long-press
escape is unaffected. This is an accepted split, not drift — do not "unify" it.

## 2. Global live adjustments — speed, volume, pitch

These are exercised in every mode, so they must be identical everywhere:

- **Speed (WPM):** changed by **turning the encoder while it owns speed**
  (its default role), via `changeSpeed()`. The displayed WPM updates immediately
  in the standard top-bar slot (§4). Modes that deliberately manage speed
  themselves (Echo Trainer adaptivity, Morsel ramp-down) vary *relative to* the
  user's setting and **restore it on exit**. Games must route in-game speed
  through `changeSpeed()` too (target — `divergences.md` H3).
- **Volume:** **red single-click selects volume**, then the encoder adjusts it
  via `changeVolume()`; a second red click returns the encoder to speed and
  persists the value (`writeVolume()`). A mode may never trap or repurpose this
  gesture.
- **Pitch / tone:** set in **preferences only** (`posPitch`), never per-mode.
  Confirmed: no mode exposes a live pitch control.
- **Entering the preferences menu:** **black-knob double-click**, from any classic
  running state and from the QSO Bot. In Generator/Echo it stops the current run
  first. A game that cannot allow it mid-round offers it from the lobby/result
  screen instead.
- **Persistence:** volume, brightness, file-player word pointer, last-executed
  menu item, game high scores, and player identity are written **immediately**;
  all `pliste[]` preference values are written **on preferences exit**; WPM is
  held in RAM during a session and persisted with preferences/snapshots.

## 3. Lifecycle of an interactive mode

1. **Entry** from the menu shows the mode's start state via `showStartDisplay()`.
   Confirmed classic behavior: **Keyer, all Transceiver modes, and Decoder are
   immediately active**; **Generator and Echo Trainer start "armed"**, waiting
   for a paddle touch or black-knob click to begin generating. Games always show
   an explicit **start screen / lobby** — nobody is dropped into a running game.
2. **Start** in games must use **one gesture across all games** (target: the
   classic "paddle/key to start"). Today this is not unified (Invaders = paddle,
   Radio Cave/Pileup = click, Morsel = either) — `divergences.md` H1.
3. **During operation,** top bar and content area follow §4 and §5.
4. **Interruption:** modes with resumable state (e.g. Radio Cave) save to NVS
   automatically on exit — the user is never asked "save? y/n" on the device.
5. **End of a game round** shows a **result screen**: outcome, score, and the
   two standard choices — *play again* / *back to menu* — mapped to the same
   inputs and worded identically in every game (target wording: `Click: Play
   Again` / `Long press: Exit`; today the casing/vocabulary drifts —
   `divergences.md` M2). Classic modes have no rounds; they run until exited.
6. **Exit** always returns to the same place in the menu the user came from.

## 4. Top bar

- The top bar always shows the **mode identity** plus the most useful live
  values (typically WPM; in games possibly score or — Radio Cave — the mini map).
- **Shared slots — the confirmed classic standard** (`updateTopLine()` /
  `displayCWspeed()`), by character column:

  | Col | Content |
  |---|---|
  | 1 | `x2` / `<>` indicator (Generator: word-doubler / auto-stop) |
  | 2 | keyer-mode letter `A B U N S`, or `d` (Decoder) |
  | 3 | `SK` / `CK` (straight key vs iambic) |
  | 3–6 | effective/Farnsworth WpM `(NN)` in Gen/Echo, `rNNs` rx-wpm in Trx |
  | **7–8** | **WpM value — bold when the encoder owns speed** (the shared speed slot) |
  | 10–12 | `WpM` label |
  | right | volume bar; LoRa/WiFi logo while transmitting |

- The top bar is the only chrome in classic modes; the scroll area below is
  reserved for content. Games may use the area below freely (TFT graphics) and
  may replace the bar with a bespoke HUD, but a value shared with the classic
  modes (WPM above all) should keep the standard meaning/format even in a HUD.
  The QSO Bot uses the classic top bar unchanged.

## 5. Text and scroll area (classic modes; games where they show text)

- Output uses the shared scroll area with `NoOfVisibleLines` visible lines
  (3 OLED / 4 TFT); wrapping is done by `DisplayWrapper`, never by the display
  library.
- **CW-source distinction (confirmed mechanism):** machine/incoming CW is
  `REGULAR` weight (`FONT_INCOMING`), user-outgoing CW is `BOLD`
  (`FONT_OUTGOING`); result tokens `OK`/`ERR` are `BOLD`. **Target:** apply this
  same distinction — weight on mono displays, a palette role on TFT — uniformly
  across Echo Trainer, Transceiver modes, QSO Bot, and games (today the games
  use their own colors with no shared rule — `divergences.md` M6).
- Prosigns are displayed in their conventional form — `<as> <ka> <kn> <sk> <ve>
  <bk> <err>` (from `cleanUpProSigns()`) — and are never broken across lines.

## 6. CW behavior

- All machine-generated CW respects the **user's configured speed**; where a
  mode varies speed deliberately (Echo Trainer adaptivity, Morsel ramp-down),
  the variation is relative to the user's setting, explained in the manual, and
  the setting is restored on exit.
- **Koch filtering (confirmed):** only the **Koch Trainer** sub-modes filter the
  character set to the selected lesson (the `kochActive` flag). **CW Generator
  and Echo Trainer deliberately do *not* Koch-filter** — they are the "free"
  trainers and honor the chosen content type / random option instead. *(This is
  intentional; do not treat it as a bug.)* Among games, Morse Invaders and
  Morsel are Koch-lesson aware; Radio Cave and Fight the Pileup are
  content-driven.
- Sidetone pitch and volume always follow the global preferences; no mode sets
  its own tone parameters. All CW audio goes through `MorseOutput::pwmTone`.
- QSO-style exchanges (QSO Bot, Radio Cave's final QSO, Transceiver practice)
  follow **real CW operating conventions** — correct prosigns, plausible exchange
  structure, standard abbreviations. When in doubt, ask Willi; he is the domain
  authority.

## 7. Scores, progress, and persistence

- High scores persist in **NVS**. **Current reality:** scores are split across
  three namespaces (`m32game` = Invaders, `morserino` = Morsel, `radiocave` =
  Radio Cave save; Fight the Pileup persists none). **Target:** one canonical
  scheme with a schema version and a one-time migration (CLAUDE.md §4,
  `divergences.md` M5/L5).
- The result screen always shows this round's result and the relevant high
  score, in the same layout across games.
- Resumable modes save on exit and offer **Resume / New game** on next entry —
  same wording, same input mapping, wherever it appears.
- **Score reset (target):** one uniform mechanism. Today only Radio Cave has an
  in-game `NEW` (+`Y`) wipe; the other games have none, and `resetDefaults()`
  only clears `morserino` (`divergences.md` M1).

## 8. Timeouts and errors

- The global TOT (inactivity shutdown) applies in all modes; any user activity
  resets it.
- **Per-round timeouts inside games are deliberately game-specific** *(intentional)*.
  Fight the Pileup shows and enforces an explicit per-attempt timeout; other
  games have none. Where a game has one, it is communicated before it first
  matters and expiry produces a consistent in-game result. There is no global
  "timeout = miss" rule, by design.
- Error/edge messages are short, English, one tone: factual, no exclamation
  marks.

## 9. Multiplayer (ESPNow / LoRa / WiFi)

Two patterns exist and are **not** yet unified:

- **Classic Transceiver** (LoRa / WiFi): the peer is configured in preferences
  (`wlanTRXPeer`); the session starts on mode entry (WiFi resolves/broadcasts to
  the peer, LoRa just receives). No interactive pairing screen.
- **Game multiplayer** (Morsel, Fight the Pileup over ESP-NOW): an explicit
  **lobby** — "Start as Server" / join, roster, "waiting for server" — set up via
  `setupESPNow()` after a boot-time `wifiWarmup()`.

The classic Transceiver flow is the incumbent; where game multiplayer concepts
overlap (initiating a session, showing the peer, handling disconnect) they
should feel like it. Convergence is a design decision for Willi
(`divergences.md` B20).

## 10. Settings

- Global, cross-mode settings are reached and edited via the **preferences
  mechanism** (black-knob double-click → `setupPreferences()`), the same for
  every classic mode and the QSO Bot.
- **Game-specific settings may live in the game's lobby** *(intentional, §12)* —
  but any setting shared with the classic modes (speed, volume, pitch, theme)
  must remain editable through the normal preferences menu, not be trapped in a
  lobby. Some game settings already are real preferences (`posInvaderOrient`,
  `posTheme`, `posQsoBotContestType`); the split of "which setting goes where"
  should be made deliberate (`divergences.md` M8).
- Settings changes take effect the next round at the latest, and persist.

## 11. Manual parity

Every convention here corresponds to a sentence in the user manual. When a
convention changes, both the English and German manuals change with it. The
section structure of this document is intended to map onto the manual's
chapters, so keep headings stable.

## 12. Games — control grammar (target spec)

Games own the screen but **not** the control grammar. The target each game must
reach (tracked in `devdocs/REFACTORING_PLAN.md` Phases D/F):

- **Exit:** black-knob long-press only. No red-button exit/forfeit overload.
- **Start:** one gesture for all games (recommended: paddle/key to start).
- **Speed / volume:** the §2 mechanism exactly — encoder owns speed by default,
  red single-click toggles to volume, routed through `changeSpeed()`/
  `changeVolume()`; display the WPM the standard way even inside a HUD.
- **Preferences:** offered from the lobby/result screen (mid-round exemption is
  allowed); shared settings still reachable from the normal preferences menu.
- **Result screen:** identical wording and input mapping across games —
  `Click: Play Again` / `Long press: Exit`.
- **Pause** (where it exists): black-knob single-click, mirroring the trainer
  start/stop feel.

What games **may** differ in: layout, graphics, color, HUD content, lobby design,
game-specific settings, and per-round timeout rules (§8). Everything in §1–§2
is mandatory and shared.
