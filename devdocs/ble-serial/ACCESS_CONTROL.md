# BLE Serial — access control

*Design note for sign-off, 2026-08-19. Written before implementation, at
Willi's request. Decided: **physical confirmation on the device, plus a
connection indicator**. Implementation belongs on its own branch off `master`
(CLAUDE.md §1).*

## The problem

BLE Serial has no access control. Any central within ~10 m that speaks the
Nordic UART service can send `put device/protocol/on` and then drive the full
protocol. An App Store release of the iOS config app changes this from
theoretical to likely: it puts a ready-made client in every pocket at a club
meeting or a hamfest.

**And today the device gives no sign it happened.** `onConnect` in
`MorseBleSerial.cpp` sets flags and returns — no display, no tone. The operator
cannot currently detect the situation, let alone refuse it.

### Sized honestly

Reachable over an unauthenticated session: preferences, CW memories, WiFi
SSIDs, file upload to SPIFFS, call sign and name, reset-to-defaults, game
scores, practice stats. WiFi passwords are write-only by design, so nothing
secret is readable, and a call sign is public by definition for a licensed
operator. **The damage ceiling is nuisance**, not harm — someone resets your
keyer settings or uploads a rude MP3.

Three properties already narrow the window: BLE Serial is not on by default
(option 5 of the *Bluetooth Use* selector), only one central may be connected
at a time, and range is short. What is missing is any way to say *no*.

## Why not a password (the alternative considered)

The first proposal was a device-stored password with a challenge/response
exchange: device sends a nonce, client hashes nonce+password, device verifies.
The construction is sound — the secret never crosses the air and a fresh
single-use nonce defeats replay — but it was **not chosen**, for four reasons
worth recording so they are not relitigated:

1. **Offline dictionary attack.** One captured `(nonce, hash)` pair lets an
   eavesdropper brute-force a human-chosen password at leisure; BLE sniffing
   hardware costs about €40. Resisting it needs a deliberately slow KDF, which
   the ESP32 must also compute on every connect.
2. **It authenticates the connection, not the session.** Traffic after the
   handshake stays in the clear either way.
3. **Enrolment window.** Setting the first password over the air exposes it.
4. **Password management is the real cost** — entry via the encoder character
   picker, and a forgotten-password recovery path that is a backdoor by another
   name.

**The decisive argument for presence instead:** over BLE, *range and physical
presence are the same thing*. The legitimate operator is always within arm's
reach of the device being configured. Proof-of-presence therefore costs them
one button press and costs a remote attacker everything — with no secret to
store, no NVS entry to budget, no recovery path, and no enrolment window.

Keep the shared-secret design in reserve for the case it actually fits: a
club-shack device where someone had legitimate physical access once and should
not retain access afterwards. That is the scenario where a secret beats
presence, and it is not the scenario we are in.

If session *confidentiality* is ever wanted, the answer is BLE bonding with
Passkey Entry (link-layer encryption, MITM protection, iOS supplies the
dialog), not an application-layer password. That is a larger job with real NVS
cost and a history of bond interference in this codebase (the static random
address exists because old *keyboard* bonds hijacked the link), so it is a
later step, not this one.

## The design

### 1. Where it hooks in

The gate is the pre-handshake branch of `bleSerialEvent()` in `m32_v6.ino`:

```cpp
if (!bleProtocol) {                       // only protocol/on is recognized here
    if (bleInputString.equalsIgnoreCase("put device/protocol/on")) { ... }
```

That branch is already **BLE-only**, which gives the right answer for free:
**USB is exempt.** A USB cable is itself proof of presence, and gating it would
break every existing host for no gain.

Confirmation is tied to the **protocol handshake**, not to the BLE connection.
A bare connection can do nothing, so prompting on connect would produce noise
for anything that merely scans and attaches.

### 2. The prompt

Modelled directly on the one existing yes/no confirmation in the firmware,
`MorsePreferences::resetGameScores()` — same three-line layout, same input
mapping, same blocking loop with `checkShutDown(false)` inside it:

```
Allow connection?
FN = yes
click = no
```

- **FN (red button) = allow, encoder click = deny.** Reusing the existing
  mapping means the gesture is already learned. (Open question O1 below asks
  whether a security prompt should instead demand something more deliberate.)
- **A timeout declines.** ~20 s, then treat as denied. `resetGameScores()`
  blocks indefinitely; this cannot, because the requester may simply walk away.
  **Fail closed:** only an explicit positive gesture admits a client.

### 3. Only at the top menu

A request that arrives while the device is in an interactive mode is
**declined immediately**, without a prompt. Three reasons converge:

- **Blocking is only safe when idle.** The `resetGameScores()` pattern blocks
  the main loop. Doing that mid-Keyer or mid-Generator would freeze CW
  generation — unacceptable.
- **No gesture collision.** In a running mode the black-knob click already
  means start/stop or select-keyer-memory (UX §1, §1a). A modal that borrows it
  would make an accidental approval one careless click away.
- **It is additional security**, not just simplification: a device in use
  cannot be taken over at all.

Cost to the legitimate user: connect from the top menu, then navigate wherever
you like — the link persists. Realistically that is already when people
connect.

### 4. Telling the client what is happening

The app's handshake timeout is 2 s × `BLE_TIMEOUT_FACTOR` (3) = 6 s. A 20 s
confirmation window would time out on the client first, so the exchange needs
**two stages**:

1. On raising the prompt, the device immediately answers (BleOnly scope)
   `{"message":{"content":"CONFIRM ON DEVICE"}}`. The client shows *"Press FN on
   your Morserino to allow"* and extends its wait.
2. On the operator's answer, the device sends either the normal
   `{"device":{...}}` handshake reply, or `{"error":{"content":"CONNECTION
   DECLINED"}}`.
3. A request arriving mid-mode is answered `{"error":{"content":"DEVICE
   BUSY"}}` at once, so the app can say *"return the device to its main menu
   and try again"* instead of timing out blind.

Client-side note: the tool's `doConnect()` treats any reply without `.device`
as "no protocol handshake" and prints its generic hint. It needs to surface
`error.content` instead — a small change in the shared tool, benefiting USB too.

### 5. The connection indicator

A BLE glyph in the top bar while a client is attached, following the existing
`MorseOutput::dispLoraLogo()` / `dispWifiLogo()` precedent and living in the
same right-hand region UX §4 already reserves for them ("volume bar; LoRa/WiFi
logo while transmitting"). Character columns 1–12 are fully allocated and must
not be disturbed.

This is worth having **independently of the prompt**: it turns an invisible
event into a visible one, and it is what tells the operator a session is still
open.

## Conformance with UX_CONVENTIONS.md

**§10.1 must be amended.** It currently states that for background connectivity
services "Connection and disconnection are silent on the device screen". This
design deliberately breaks that for BLE Serial. Proposed replacement wording:

> Connection and disconnection are silent on the device screen, except that a
> service which grants an outside party control of the device requires the
> operator's explicit consent: a connection request is confirmed on the device
> (FN = yes) and refused if unanswered, and an established session is shown by
> an indicator in the top bar. Requests arriving while an interactive mode is
> running are refused without prompting.

Everything else conforms as-is: no global gesture is redefined (§1); the prompt
borrows the confirmation idiom already in the firmware; the message is short,
factual and English with no exclamation mark (§8); and the top-bar indicator
respects the shared column allocation (§4). Per §11, both manuals change with
this.

## Accessibility (CLAUDE.md §8, UX §13)

The Accessibility Edition builds with `CONFIG_BLE_SERIAL` (it inherits the
Pocket flags), so a blind operator meets this prompt too — and a prompt they
cannot hear is a lock they cannot open.

**This message cannot use the normal mechanism.** CLAUDE.md §8 case 2 says new
on-screen messages are voiced through the M32-protocol text stream — but here
the protocol session is precisely what is being authorized, and it does not
exist yet. Chicken and egg. The prompt must therefore be a **pre-rendered clip**
spoken through `MorseVoice::announce()`, the same path
`MorsePreferences::a11ySay()` uses. That call needs no `#ifdef`:
`announce()` is a no-op on builds without `CONFIG_AUDIO_A11Y`, deliberately, so
call sites stay clean (`MorseVoice.h`). The Accessibility Edition inherits
`CONFIG_BLE_SERIAL` because `pocketwroom-accessibility` extends
`env:pocketwroom` — verified, not assumed.

Strings owed (each must stand on its own, UX §13, and must name the button
because a blind operator cannot read the second and third lines):

- "Allow Bluetooth connection? F N for yes, click for no."
- "Connection allowed."
- "Connection refused."

Per UX §13 speech never gates an action: the clip plays while the prompt is up
and the buttons are live throughout — the operator may answer before it
finishes. Disconnection stays silent; the confirmation itself is the
announcement, and narrating every session end would be noise.

## Implementation constraints

- **Never block in the Bluedroid callbacks.** `onConnect`/`onDisconnect` run on
  the host task and must only set flags — the prompt is raised from the loop
  task behind a pending flag, exactly as `advertisePending` already defers
  advertising restarts.
- **The prompt loop must keep pumping.** `MorseBleSerial::pump()` drains the TX
  ring and handles session resets; a 20 s wait that never pumps would stall the
  link and could overflow the ring. Pump, but do not dispatch: no inbound line
  may execute while the session is unconfirmed.
- **NVS cost: zero.** Nothing is persisted. This is the main practical advantage
  over the password design, given the §4 budget.
- **`bleProtocol` stays false until confirmed**, so the existing invariant
  ("pre-handshake, only protocol/on is recognized") continues to do the work of
  rejecting everything else.

## What this does and does not protect

**Does:** stop a stranger from taking control of a device they cannot touch.
That is the whole of the stated threat.

**Does not:** protect the traffic. An eavesdropper with a sniffer still reads
an authorized session in the clear — call sign, WiFi SSIDs, CW memories. Only
link-layer encryption (bonding) fixes that, and nothing here forecloses adding
it later.

## Decisions (signed off 2026-08-19)

1. **D1 — gesture: FN click allows, encoder click denies.** The mapping
   `resetGameScores()` already uses. One learned idiom; no new convention.
2. **D2 — top menu only.** A request arriving during any interactive mode is
   refused with `DEVICE BUSY` and no prompt at all.
3. **D3 — tolerate existing clients.** A V9-era client that gives up while the
   prompt is up is not punished: the prompt stays open, and once the operator
   presses FN the client's next connect attempt handshakes normally. No protocol
   version gate — the pre-handshake state cannot be negotiated anyway (a client
   would have to handshake to learn it needs to handshake differently).
4. **D4 — no opt-out preference.** A setting to disable confirmation is also a
   setting an attacker can switch off once inside. Add it only if the prompt
   proves tiresome in practice.

### Why D3 matters more than it looks

The pre-reply `{"message":{"content":"CONFIRM ON DEVICE"}}` **breaks an existing
client's first attempt**: the config tool's `doConnect()` parses any reply,
finds no `.device`, and declares failure. That is a behaviour change, not a
purely additive one, so "additive within a major version" does not cover it.
D3 makes the failure recoverable rather than terminal — old client fails once,
operator presses FN, next attempt works — and updated clients (which understand
the pre-reply and extend their wait) never see it at all.

## Open questions — all resolved

O1–O4 were answered on 2026-08-19; see **Decisions** above. Kept here only as a
pointer, so a reader arriving from an old link is not left looking for them.
