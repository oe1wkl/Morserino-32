# Manual draft — Remote Keyer / Remote Rig (EN)

**Status: draft for review, not yet inserted into the manual.** Revised 2026-09-17 after Willi's
four review points: the preferences now really do open on the rig's items, the *Rig Hang Unit*
preference is gone (milliseconds only), and the ARRL name for the first-element delay is given.

Two blocks go into `Documentation/User Manual/Version 9.x/manual_en.md`:

- **Block A** — one `###` section inside `## Transceiver`, immediately after `### iCW/Ext Trx`
  and before `### QSO Bot`. That is where the two menu entries sit in the mode ring.
- **Block B** — one new `###` section inside `## List of All Morserino-32 preferences`,
  after `### Preferences regarding Transmitting, Decoding and QSO Bot`.

The German manual gets the same two blocks, translated. Neither is written yet.

---

## Block A

### Remote Keyer and Remote Rig

These two modes turn a pair of Morserinos into a remote keying link: you key one of them at
your operating position, and the other one — at the station, which may be in the next room or
in another country — keys the transmitter. What comes out of the transmitter is what you keyed,
with your own timing: dits, dahs and spaces are reproduced as you made them, whether they came
from the paddles, from a straight key or from a bug.

The mode is a keying link, not an audio link. You hear your own sidetone locally and
immediately, so nothing on the network can disturb your sending rhythm. What you hear *from*
the station — the band, your own signal, the other operator — is not carried by the Morserino;
use whatever remote-control or audio path your station already has for that.

The Morserino at your position runs **Remote Keyer**. The one at the station runs **Remote
Rig** and keys the transmitter through its "to Tx" connector, exactly like the other
transmitting modes. The link uses UDP port 7374.

#### What you need {-}

- **Both Morserinos on a network.** A local network is enough for a test; for real remote
  operation the rig end must be reachable from the keyer end over the Internet. The same
  considerations as for **WiFi Trx** apply: if the rig is behind a router doing NAT, you need
  port forwarding for UDP port 7374 to the rig's Morserino, or a VPN that puts both on one
  virtual network.
- **The address of the rig**, entered at the keyer end as the **TRX Peer** field of
  **Config WiFi** — the same field the WiFi transceiver uses. An IP address or a DNS host name.
- **A pass phrase, the same on both devices.** Enter it in the **Config WiFi** web form, in the
  field *Remote Keying pass phrase?*, at least 12 characters. It is never shown back to you,
  exactly like the WiFi passwords: leave the field empty to keep the stored one.

The pass phrase is not a nicety. Every packet of the link is authenticated with it, and a
Morserino in Remote Rig ignores anything that does not carry the right signature — which is
what stops a stranger on the Internet from keying your transmitter. Choose something long, and
give it to your remote partner by a channel you trust. Neither device will start the mode
without it.

#### At the station: Remote Rig {-}

Select **Remote Rig** from the menu. The device briefly shows *Unattended — keep on power*,
then its own IP address on the top line, and waits, showing **Listening**.

This mode is meant to be left alone: it never goes to sleep, and the display timeout does not
apply. Nothing at the device itself can key the transmitter — the paddles, the touch pads and
the key jack are deliberately dead here, so that a cat, a visitor or a knocked table cannot put
a carrier on the air.

While a session is running, the display shows whether the key is down, how many edges arrived
late, how many times the key line ran dry, and the speed the rig is seeing. When a session
ends, the reason stays on the screen until the next one starts:

| Shown | What happened |
|---|---|
| End: BYE | the keyer end left the mode normally |
| End: timeout | nothing was heard from the keyer for a second, and it did not come back |
| End: errors | too many damaged packets — a bad path, or something on the network is not what it claims to be |
| End: overflow | edges arrived faster than they could be played out |
| End: reset | you pressed the encoder at the rig |

Two controls: a **short press of the encoder** ends the current session (the keyer end notices
and calls again within a second — this is the way to recover a link that has gone strange), and
a **long press** leaves the mode, as everywhere else on the Morserino.

Whatever ends a session, the key line is released first. A mark that was on the air when the
link died does not stay there.

#### At your position: Remote Keyer {-}

Select **Remote Keyer**. It calls the rig, and once the rig answers you can key: the mode
behaves like **CW Keyer** in every other respect: what you key appears on the display, the keyer
memories are on the black knob, speed and volume are where they always are. The
WiFi symbol on the top line means the rig is answering; if it disappears, the link is down, and
the Morserino goes on calling once a second until the rig comes back. You can go on keying
while it does — you will simply not be transmitting.

Your own "to Tx" connector stays quiet in this mode. The Morserino in front of you keys nothing
locally, whatever **Key ext TX** is set to; the transmitter that gets keyed is the distant one.

#### The delay, and why there is one {-}

A network does not deliver packets evenly. If the rig keyed the transmitter the moment each
edge arrived, the jitter of the path would land directly on your CW and deform it. So the rig
holds every edge for a fixed time — the **Rig Delay** — and then plays it out on its own clock.
The delay is the same for every edge, so the CW that leaves the transmitter has exactly the
timing you gave it; only the whole transmission is shifted by that delay.

The default, **Adaptive**, lets the rig find a delay that suits the path and raise it when edges
start arriving late. A fixed value is the more predictable choice on a path you know: pick one
comfortably larger than the worst delay variation you see. On a good local network 50–100 ms is
plenty; across a continent, 200–300 ms is more realistic. A chosen value is also a floor — the
rig may raise it when the path gets worse, and will come back down to your choice afterwards.

The delay does not affect your sidetone, which is local and immediate. It affects only how long
after your key movement the distant transmitter follows, and it is the price of clean CW at the
far end.

#### Break-in compensation {-}

If your transceiver is keyed without a PTT line, it works in semi break-in ("VOX" for CW): the
first key-down switches it from receive to transmit, and the time it takes to change over is
clipped off the beginning of that first element. The ARRL bench tests call this the
transceiver's **first dit on delay**; on many rigs it is a few milliseconds, on some
considerably more. After a pause — between words, or when you come back after listening — the
rig has dropped back to receive, so the next first element is clipped again.

The Morserino can compensate for this better than a real-time keyer can, because it knows every
edge one **Rig Delay** in advance: it **starts the first element early and leaves its end where
it is**. The element that goes on the air is the one you keyed; everything after it is
untouched, and the spacing of your CW is not disturbed.

Two settings at the rig end:

- **Rig 1st Ext** — how much earlier that first element starts, in milliseconds. Set it to the
  transceiver's own changeover time. If you do not know it, start at 5–10 ms, which suits a rig
  with a relay, and listen: too little leaves the first dit of a word short, too much makes it
  long. 0 switches the compensation off.
- **Rig Hang** — the transceiver's own break-in delay, in milliseconds, as its manual states it
  (the setting is in 50 ms steps). This is how the Morserino knows when the transceiver has
  dropped back to receive and the next element will be clipped. Set it too short and elements
  get lengthened that did not need it; set it too long and a genuine changeover is missed. In
  either case the error is at most the extension, on one element after a pause.

If your transceiver is keyed through a PTT line, or you run it in full break-in with no
changeover time worth mentioning, leave **Rig 1st Ext** at 0.

#### Changing the rig's settings from where you sit {-}

The settings above belong to the rig, and have to be tuned against the transmitter — which is
exactly where you are not. So while the link is up, they can be reached from the operating
position: in **Remote Keyer**, a double-click of the encoder opens the preferences as always,
and the menu opens directly on the rig's own settings, which come first in the list. Each one
carries a **Rig:** prefix, so it cannot be mistaken for a setting of the Morserino in front of
you:

**Rig: Delay**, **Rig: Lim Kyr**, **Rig: Lim SK**, **Rig: 1st Ext**, **Rig: Hang**.

They behave like any other preference. A change takes effect at the far end immediately and is
stored there, so it survives a power cut at the station. The values you see are the rig's own,
read back over the link when the menu opens.

These items appear only while the link is up. If the link drops while you have the preferences
open, the menu closes by itself and the display returns to the keying screen — rather than
leaving you editing settings that no longer reach anything.

#### Safety {-}

A key that sticks down at a remote station is a transmitter that stays on the air. Two limits
at the rig end guard against it: **Rig Limit Kyr** for a paddle keyer and **Rig Limit SK** for a
straight key or a bug. If a single mark lasts longer than the limit, the rig lifts the key. The
straight-key limit is the more generous of the two, because holding the key down to tune is a
normal thing to do — the default of 10 seconds is enough for that and short enough to matter.

The rig also lifts the key when the link goes quiet, when the session is ended from either end,
and when it is told to stop. There is no state of the link in which a mark is left on the air.

#### If it does not start {-}

| On the display | What to do |
|---|---|
| No key set — Set a pass-phrase first | Enter the pass phrase in the **Config WiFi** web form, at both ends |
| No rig host — Set TRX Peer in Config WiFi | The keyer end has no address for the rig; enter it in the **TRX Peer** field |
| Host not found | The name or address does not resolve; check it, and check that this Morserino is on the network |
| No answer — Check rig and pass phrase | The rig did not reply: it is not in **Remote Rig**, or it is not reachable (port forwarding?), or the two pass phrases differ |

---

## Block B

### Preferences regarding Remote Keying

These preferences configure the two remote keying modes (section **Remote Keyer and Remote
Rig**). The five **Rig** items are read by the Morserino that keys the transmitter, so set them
on that device — or, more conveniently, from the operating position while the link is up, as
described in that section. **Glitch Filter** belongs to the Morserino you key.

| Preference Name | Description | Values |
|---|---|---|
| Rig Delay | How long the rig holds each key edge before it plays it out, so that network jitter cannot deform your CW. **Adaptive** lets the rig find and adjust the delay by itself. A fixed value is the start value and the lower limit: the rig may raise it when edges arrive late, and returns to your choice when the path recovers. A larger value is safer on a poor path and delays the distant transmitter accordingly; it never affects your local sidetone. | **Adaptive** / 50 / 100 / 150 / 200 / 250 / 300 / 400 / 500 / 600 ms |
| Rig Limit Kyr | Safety limit at the rig: the longest single mark accepted from a paddle keyer before the rig lifts the key. A keyer cannot legitimately produce a mark of several seconds, so this can be tight. | 1 … 30 s (**3**) |
| Rig Limit SK | The same limit for a straight key or a bug, where holding the key down — to tune, for instance — is normal. Hence the more generous default. | 1 … 30 s (**10**) |
| Rig 1st Ext | Break-in compensation: how much earlier the rig starts the first element after a pause, to make up for the transceiver's changeover time (the ARRL bench tests call it the *first dit on delay*). The end of the element is not moved, so what goes on the air is what you keyed. 0 switches the compensation off — the right setting when the transceiver is keyed through a PTT line or runs full break-in. | 0 … 30 ms (**0**) |
| Rig Hang | The transceiver's own break-in delay, as given in its manual, so that the Morserino knows when the transceiver has dropped back to receive and the next element will be clipped. Only relevant when **Rig 1st Ext** is not 0. | 0 … 3000 ms in 50 ms steps (**250 ms**) |
| Glitch Filter | At the keying end: how long a contact must hold before it is taken as a real key edge. Contact bounce from a straight key or a bug is discarded; a genuine edge keeps the time at which the contact was first made, so the filter costs no timing accuracy. It is not applied to the internal keyer, whose edges are clean by construction. | 1 … 5 ms (**3**) |

---

## Notes for review (not manual text)

1. **"First in the list" is now true.** It was not when the first draft was written: the
   preferences menu resumes where it was last left, so the rig items were first in a list nobody
   was looking at. The menu now opens *on* the first rig item whenever the link is up.
2. **The hang unit is gone.** Your reading of the Icom, Yaesu and Elecraft manuals settled it —
   no manufacturer states the break-in delay in dits. The preference is removed, the value is
   entered in milliseconds, and the wire keeps a reserved byte so dits could come back later
   without a version break.
3. **The first dit *off* delay is deliberately not covered.** The ARRL tests report one, and a
   transceiver that stretches or clips the *end* of an element would need the mirror image of
   this compensation — which the Rig could do just as well, since it knows the following gap in
   advance. I have not found figures that say whether it is worth doing, and I would rather not
   write manual text around a mechanism we have not measured. Suggested course: leave it out of
   V10, and ask the testers with well-instrumented stations whether they can hear or measure a
   shortened last element. If they can, it is a small addition (one more preference, the same
   machinery running the other way).
4. **Not covered on purpose:** the protocol details (they are in the serial-protocol document and
   in `devdocs/m32kip/SPEC.md`), the fixed port 7374 override, and the measurement instrument.
5. **Still owed:** the German translation of both blocks, and a decision on whether the section
   heading should read "Remote Keyer and Remote Rig" or something more descriptive such as
   "Keying a distant transceiver".
