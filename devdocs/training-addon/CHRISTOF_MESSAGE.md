# Draft: message to Christof Dallermassl, OE6CHD (tegmento)

*Drafted 2026-10-29 for Willi to edit and send himself — nothing has been sent. Decision 8 in
`LEAN_V1_SCOPE.md` §7: Christof first, before M1 starts, by direct message (not a public issue), with the
draft wire format, framed as a preview for any client, no obligation. The build does not wait for his reply.*

**Before sending, Willi:**

- Fill the two bracketed items (when the firmware work starts; the date by which feedback is most useful).
- The text avoids saying who Christof is beyond "author of the M32 CW Trainer". If he is also the Christoph
  Daller who co-designed the protocol text stream (`CLAUDE.md` §8), add a sentence thanking him for that —
  I inferred the identity from the name only.
- Left out on purpose: Oliver (OM0RX), Tommy (OZ1THC), Enzo (IW7DMH), the phrase corpus, the mastery model
  details. Each has its own moment in decision 8.
- The wire format below is the draft from `LEAN_V1_SCOPE.md` §3. If you change it before sending, change the
  scope too.

---

## English

**Subject:** M32 protocol: two small events for training apps — would you take a look?

Hi Christof,

I'd like to run something past you before I build it, because your M32 CW Trainer is the obvious first
client for it.

I'm planning a small addition to the M32 serial protocol (version 1.6): two new events, so that a computer
program can time and score training better than it can today.

1. `cw` / `done` — the device reports when a `PUT cw/play` has finished sounding (or was stopped). Today a
   program has to guess when to start its clock.
2. `trainer` / `result` — in the Echo Trainer, one line per evaluated word, with the device's own timing
   (latency to the first paddle closure, duration of the answer).

Draft wire format:

```
{"cw":{"event":"done","reason":"end"}}                    reason: "end" | "stopped"
{"trainer":{"event":"result","mode":"echo","prompt":"cq","answer":"cq","ok":true,
            "repeat":0,"wpm":18,"koch":12,"latency":820,"duration":2100}}
```

`latency` and `duration` come from the device's clock, so link jitter does not enter. If the think time
runs out, `answer` is empty and `"timeout":true`. Both events are always on (no opt-in command) and
announced by a new entry `trainer-events` in `GET capabilities`. The protocol description already tells
clients to ignore objects they don't know, so nothing should break — but you know your parser better than I
do, and an event arriving in the middle of the raw character stream is the case I'd most like you to check.

Two questions: would you use these? And is there something you'd want in them that I'm missing (a
timestamp, an event per generated word, …)? Nothing is frozen until it has been through the bench, so this
is a good moment to ask.

And, openly: I'm also planning a trainer web page of my own on top of these events — Koch copy training with
a per-character mastery model, phrases as well as words, answers typed or keyed on the paddle. That
overlaps in part with yours. It is not meant to replace it; yours is the one learners already know. If you
would rather talk about working together, or about sharing ideas or code, I'm very open to that. The events
are for any client either way.

No hurry. I'll start the firmware work [in the coming days / around DATE]; feedback before [DATE] would be
ideal, but I won't hold things up waiting for it.

73,
Willi OE1WKL

---

## Deutsch

**Betreff:** M32-Protokoll: zwei kleine Events für Trainings-Programme — magst du einen Blick darauf werfen?

Hallo Christof,

ich möchte etwas mit dir abstimmen, bevor ich es baue, denn dein M32 CW Trainer ist der naheliegende erste
Nutzer davon.

Ich plane eine kleine Erweiterung des M32-Protokolls (Version 1.6): zwei neue Events, damit ein
Computerprogramm das Training genauer messen und auswerten kann als heute.

1. `cw` / `done` — das Gerät meldet, wann ein `PUT cw/play` fertig gegeben (oder abgebrochen) wurde. Heute
   muss ein Programm raten, wann es seine Uhr starten soll.
2. `trainer` / `result` — im Echo Trainer eine Zeile pro ausgewertetem Wort, mit der Zeitmessung des Geräts
   selbst (Latenz bis zum ersten Paddle-Kontakt, Dauer der Antwort).

Entwurf des Formats:

```
{"cw":{"event":"done","reason":"end"}}                    reason: "end" | "stopped"
{"trainer":{"event":"result","mode":"echo","prompt":"cq","answer":"cq","ok":true,
            "repeat":0,"wpm":18,"koch":12,"latency":820,"duration":2100}}
```

`latency` und `duration` kommen von der Uhr des Geräts, die Übertragung spielt also keine Rolle. Läuft die
Bedenkzeit ab, ist `answer` leer und `"timeout":true`. Beide Events sind immer eingeschaltet (kein
Einschaltbefehl) und werden in `GET capabilities` mit dem neuen Eintrag `trainer-events` angekündigt. Die
Protokollbeschreibung sagt schon jetzt, dass Programme unbekannte Objekte ignorieren sollen — es sollte also
nichts kaputtgehen. Du kennst deinen Parser aber besser als ich; den Fall, dass ein Event mitten im rohen
Zeichenstrom ankommt, würde ich gern von dir geprüft wissen.

Zwei Fragen: Würdest du die Events verwenden? Und fehlt dir etwas darin (ein Zeitstempel, ein Event pro
erzeugtem Wort, …)? Festgelegt ist nichts, bevor es auf dem Prüfstand war — jetzt ist also ein guter
Zeitpunkt.

Und ganz offen: Ich plane auf diesen Events auch eine eigene Trainer-Webseite — Koch-Hörtraining mit einem
Beherrschungsmodell pro Zeichen, mit Phrasen und nicht nur Wörtern, Antworten per Tastatur oder mit dem Paddle
gegeben. Das überschneidet sich teilweise mit deinem Trainer. Es soll ihn nicht ersetzen; deiner ist der,
den die Lernenden schon kennen. Wenn du lieber über eine Zusammenarbeit reden möchtest, oder über den
Austausch von Ideen oder Code: sehr gern. Die Events sind in jedem Fall für alle Programme gedacht.

Keine Eile. Ich beginne mit der Firmware [in den nächsten Tagen / etwa am DATUM]; eine Rückmeldung vor dem
[DATUM] wäre ideal, aufhalten lasse ich mich dadurch aber nicht.

73,
Willi OE1WKL
