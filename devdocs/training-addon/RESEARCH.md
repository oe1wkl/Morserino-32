# Training add-on for the M32 — landscape research

*Compiled 2026-10-28; §1b (IW7DMH and OZ1THC read in full) and items 18–19 added 2026-10-29. Purpose: an overview of (1) the add-on programs that exist for the
Morserino-32, (2) general CW training programs and what they offer, and (3) what could be
reused — so Willi can decide whether to build a comprehensive training add-on.
No decision is recorded here; the open questions are in §7.*

**How reliable is this?** Web research only, no hands-on testing of any third-party program.
Sources are the projects' own pages where they could be fetched; where a page could not be
read (marked *unverified*), the claim comes from a search snippet or a directory listing.
Dates are from the pages where shown, otherwise "undated". Nothing here has been run.

---

## 1. Add-ons that exist for the Morserino-32

[morserino.info](https://www.morserino.info/) lists the following as third-party training
software, plus our own tools. All the third-party ones sit **outside** the firmware and talk to
the device over USB serial (the raw character echo, governed by the *Serial Output* preference)
or over WiFi (MOPP).

| Program | Author | Platform | Link to M32 | State |
|---|---|---|---|---|
| **M32 CW Trainer** (tegmento.org; repo `cdaller/morserino32-trainer`) | Christof Dallermassl OE6CHD / 43bits | Browser (Web Serial): Chrome, Edge, Opera | USB, *Serial Output = Everything* | Active-looking: 257 commits, Apache-2.0, 21 stars; undated |
| **CW Trainer for Morserino 32** | Enzo IW7DMH | Desktop (Qt): Windows x64, Ubuntu 20.04/22.04; Raspberry Pi "in test" | USB serial; *Serial Output* Keyer / Decoder / Keyer+Decoder | Release page ≈ Oct 2021 (image timestamps); closed source, Dropbox downloads. **Send-direction keying trainer — read in full, §1b** |
| **Morserino Phrases Trainer** | Tommy OZ1THC | Windows exe (C# / WinForms, NAudio) | USB, 115200 baud, decoded stream from CW Keyer | Releases 1.0 (4 Feb 2020), 1.1 (8 Feb), 2.0 (20 Mar 2020); C# source on GitHub for 1.0/1.1 only; no licence; repo's only recent activity is a README edit (10 Mar 2026). **Send-direction phrase trainer — read in full, §1b** |
| **QSO Bot** (qsobot.online) | John M0STQ | Online service | MOPP over WiFi | Site's last post Jan 2021; docs pages not retrievable |
| **qsosrv** | pavian57 | Python / MicroPython (Pi, ESP8266) | WiFi Trx; MOPP | 66 commits, undated; README says connection steps "TBD" |
| **m32chat** (Rust) / `m32_chat_server` (Python, sp9wpn; fork by oe1wkl) | Martin / sp9wpn | Server you host | UDP, port 7373, WiFi Trx | v1.0.0, Oct 2023 |
| **Morse Trainer Pro** ([morsetrainerpro.com](https://morsetrainerpro.com/), formerly om0rx.com/morserino) | Oliver Bross OM0RX | Web + iPhone/iPad apps; free account, Premium tier | **Morserino TX Trainer** (Web Serial, decoded text only) plus a **Morserino Config** "Field Console" that also flashes an unofficial OM0RX firmware (hands-on findings in §1a) | Very active: iOS 1.3.5 on 17 Oct 2026; see §1a |
| **VBand**, **iCW** | — | Web | M32 as BLE keyboard (VBand); iCW: listed, no description | Listed on morserino.info only |

Not training software, but in the same space: our **Config Tool** (browser, USB), the **M32
Config iPhone app** (BLE), the web firmware installer, `joewittmer/Morserino-32-Firmware-Updater`,
and the LICW/CW-school **Koch curricula** that clubs run *on* the device through snapshots (LICW's
"Carousel" setup guide needs firmware ≥ 4.5). I found **no Android app** and no native desktop
app that is actively maintained.

### What the M32 CW Trainer (tegmento) does — the most complete one

- **Generator / Keyer copy test.** Compares what the M32 *sent* with what the user *typed*
  (Generator), or what the user *keyed* with what they meant to send (Keyer). Result colouring:
  green = correct, orange = missing, red = wrong. Needs **90 %** accuracy to advance to the next
  Koch letter. Options: hide the comparison, auto-hide text while receiving, ignore whitespace,
  speak the result.
- **Echo Trainer helper.** Teaches CW abbreviations: key one correctly, see its meaning in EN and DE.
- **QSO Trainer (beta).** A bot answers a simulated QSO in CW (reacts to name, QTH, RST, weather;
  messages must end in `kn`/`bk`/`k`/`e e`); "I'm lazy!" button shows example answers; replay /
  show as text / restart; speed 10–40 WpM, letter and word spacing.
- **Device management.** File upload/download for File Player, 8 CW memories, snapshots 1–8,
  WiFi slots, ESPNow.
- **CW School Graz.** One click programs snapshots 1–8 with a lesson plan (Farnsworth and Koch variants).
- **Progress.** Results stored in the browser's `localStorage`; success graph and result list.
  Not shared across browsers or devices.
- **Roadmap on its README** (not implemented): full remote control; speech output for blind users
  (the README quotes a blind user); "Play Texts" (QSO examples, user texts, type-along or listen-only);
  QSO Bot improvements (callsigns, weather, sign-off); a keyer button; fix voice comparison stopping
  after 16 s.
- **Friction it documents:** *Serial Output* must be set to "Everything" **per snapshot**; first connect
  reboots the ESP32 (DTR/RTS handling, 5 s recovery); Chrome-family only (Web Serial) — so no iOS,
  no Firefox/Safari.

### What the other add-ons add

- **IW7DMH trainer and OZ1THC Phrases Trainer:** both are keying (send-direction) trainers — see §1b.
- **qsosrv / QSO Bot:** a software *QSO partner* over WiFi — CQ → callsign exchange → RST → 73; `sk`
  resets; configurable callsign length. Teaches procedure, not speed.
- **m32chat:** a UDP relay so Morserino users can chat in Morse over WiFi Trx; rate/client limits,
  idle timeout. Social, not training.

**Observation.** Apart from Morse Trainer Pro (§1a), the M32 add-ons are all the same idea: *compare
text against the stream the M32 sends.* The differences are packaging (browser vs. exe vs.
server) and direction: tegmento is copy-direction (the user types what the device played), while
IW7DMH and OZ1THC are send-direction (the user keys a target on the paddle; §1b). None of *those* offers adaptive scheduling, response-time measurement, spaced repetition,
durable multi-device statistics, or a structured daily plan. Morse Trainer Pro does list most of
these, but as a general platform in which the M32 is one input device among several — what its
Morserino support actually does is undocumented. (Inference from what I could read, not an exhaustive
audit. *An earlier draft of this file said no add-on had these features; that was wrong once
morsetrainerpro.com was found.*)

### 1a. Morse Trainer Pro (OM0RX) — the largest platform in this space

Source: the site's front page and the App Store listing. Its sub-pages (Morserino, pricing, news,
apps) returned 404 to my fetcher, so Morserino specifics and prices are unknown. I did not create an
account or use the product.

- **Maker / reach:** Oliver Bross, OM0RX. Website plus iPhone/iPad apps (first release 19 Dec 2025,
  1.3.5 on 17 Oct 2026). The iOS app is a free download with a "Yearly" in-app subscription at $24.99;
  its listing does **not** mention Morserino or any keyer.
- **Tiers (website):** no account (quick CW session, character practice, sending board, free tools);
  free account (saves sessions, more trainers, games, progress views); **Premium** (contest suite,
  Training Intelligence). Website prices not found.
- **Breadth, ~40 trainers/tools:**
  - *Learn & receive (15):* Acoustic Character Trainer, Morse Machine, Code Groups, **CW Echo Trainer**,
    Speech/Sentences/Words/Stories/Message Copy, **Head Copy Lab**, Callsign Training, CodeSnap,
    Background Training, List Studio (custom lists).
  - *Send & operate (9):* Unified TX Practice (keyboard and paddles), **Morserino TX Trainer**, Elecraft,
    WinKeyer and VBand TX Trainers, CW Rooms, AI CW Bot, First-QSO and One-on-One QSO practice.
  - *Contests (5):* Morse Runner v2 (Premium) and Classic, Pileup Hunter, Exchange Builder, Rufz Web Pro.
  - *Games (3), Tools (9):* incl. Text↔CW, WinKeyer Tools, **Morserino Config**, key directory.
- **Planning:** practice timer shared across trainers, a **weekly planner** compared with recorded
  sessions, a "Find your training path" questionnaire (experience, goals, input method).
- **Training Intelligence (Premium):** links pace, accuracy, memory and hesitation; character
  **confusion pairs**, session replays, focused practice briefs. Progress syncs across devices.
- **Also in the site menu** (names only, not opened): SRS Hub and SRS Analytics (spaced repetition),
  Teacher Classroom / Student Classroom, CW Academy, ShackCQ One, Propagation Planner, a CW key
  directory.
- **Not on the page:** Koch method (not mentioned), supported languages, and what the Morserino
  integration does with the device.
- **Name clash:** an unrelated free Windows program also called "Morse Trainer Pro" exists on
  [SourceForge](https://sourceforge.net/projects/morse-trainer-pro/) (f4hsu-emmanuel; RX/TX, Koch,
  QRM/QSB, no link to OM0RX stated). Not to be confused with the above.

#### Hands-on findings (Willi, 2026-10-28; free account; 7 screenshots; connected device on firmware 9.1)

- **Pricing (web):** guest use is free (Acoustic Character Trainer, Morse Machine, Code Groups, **CW Echo
  Trainer**, CW Speech, CW Books, Text↔CW); a free account adds saved stats, AI CW Bot, CW Academy, the
  Practice/Games/Tools menus, **CW Rooms / TX workflows** and **Training Statistics**; **Premium is
  €29.99/year** (≈ €2.50/month, with a free trial): Morse Runner v2, Pileup Hunter, Contest Exchange
  Builder, Rufz Web Pro, **Training Intelligence** and all module analytics pages. The Morserino
  features seen are **not** behind the Premium wall. (The iOS app's yearly subscription is listed at
  $24.99 — a separate price.)
- **TX Trainer = keying practice.** The site picks a word, you key it on the M32, the decoded text is
  compared; up to three attempts per word. The session report gives words correct on 1st/2nd/3rd try or
  failed, overall and per-character accuracy, speed and "effective" WpM, a letters/numbers/signs count and
  the first wrong character per word (e.g. `illegitimatised → IH … character wrong: L`). The sample
  words were long, rare dictionary words (`unfoliated`, `exuviates`, `logopedia`), i.e. no
  frequency or Koch filtering. Nothing seen suggests it uses the M32's own Generator/Echo/Koch output
  as the source for *receive* training.
- **Connection is raw decoded text.** Its own connect card: *"Input: Decoded text. Timing: read from
  decoded device output; the browser does not reconstruct raw key timing. Web Serial at 115200 baud and
  the existing Morserino preparation flow; not generic COM support."* — the same channel tegmento uses
  (the *Serial Output* echo). The "Timing Analysis" listed on the TX Trainer help page (dit/dah ratio,
  inter-element spacing, consistency metrics, sidetone) belongs to its **virtual key / keyboard** input,
  not to the M32 path. Consequence: **nobody has element-level sending analysis for the M32 paddles** —
  that remains open and needs firmware support.
- **"Morserino Config" is a Field Console** titled *M32 Pocket OM0RX Field Console — firmware flashing,
  USB configuration, training control and decoder test tools*. Its tabs mirror ours (Connection,
  Dashboard, Preferences, Snapshots, CW Memories, User, Koch, Files, M32 TTY) plus Health, CW Audio,
  Firmware Notes and Flash. Its help says it "uses the M32 Protocol"; it shows "USB serial protocol v1.4,
  Chrome/Edge". The connected 9.1 device produced a red **"Reload failed"** (cause unknown).
- **Unofficial firmware.** A banner says it is an *"alternative M32 Pocket firmware build created for
  Morse Trainer Pro field-training workflows … not the official Morserino firmware"*, credits morserino.info
  and the upstream repository, and says the user can return to official releases. The console then
  announced **"Update available: connected firmware is 9.1; this console offers 8.2-OM0RX / OM0RX
  1.1.4"** — a *lower* version presented as an update. The official site says some features need
  firmware 9.0 or later (e.g. defaults, game scores, Bluetooth connection — per morserino.info).
  The release notes are on the console's Firmware Notes tab (summarised below); I could not find the
  fork's source anywhere public.
- **Licence angle (not legal advice):** `Software/LICENSE` is GPLv3. If the console or the firmware is
  derived from our code, GPLv3 terms apply to what they distribute. Their disclaimer and credit are
  fair; whether source is offered is something to *ask*, not to assume.

#### The OM0RX firmware, from its Firmware Notes tab (saved page, 2026-10-28)

- **What it is:** original **M32 8.2** plus an OM0RX suffix, **M32 Pocket (ESP32-S3) only**, release
  **1.1.4** (1.0 → 1.1.4 since the first field-trainer release). Flash 2,843,329 of 3,342,336 bytes used.
  It reports protocol **1.4** plus an extra `om0rxRelease` marker; it cherry-picks upstream fixes (e.g.
  sidetone-volume persistence) rather than tracking releases.
- **Stripped out to make room:** Bluetooth (stack, serial, HID, VBand keyboard), WiFi / HTTP server /
  captive portal, ESP-NOW, LoRa, and all games. A "no-QSO" smaller variant also exists.
- **Added (all on the device):**
  - **Callsign Trainer V2** — imported real-callsign pools (**253,774** calls; **3,269** CWops members),
    length filters 3/4/5/6+, portable pools and suffix modes, prefix focus and **Prefix Drill**,
    **hard similar-call mode**, Weak Copy, repeat-last, copy/listen modes with delayed reveal,
    effective-WpM / Farnsworth and inter-call delay, score tracking, and a **missed-callsign review
    queue** that re-injects missed calls at the next session start.
  - **SOTA/POTA Trainer V2** — region-specific curated refs (44 summits, 33 parks, 18 POTA states/entities),
    realistic CQ and 5NN exchanges, optional summit-to-summit practice.
  - **Keyer+Decoder** main-menu mode — half-duplex RX/TX with TX Hold, auto-TX on first paddle touch,
    external transmitter keying; **Audio Decode Test** with an RX calibration assistant; an on-device
    **HW Status** menu.
  - Data tables: 4,901 Oxford-style weighted words (`CONFIG_ENGLISH_OXFORD=1`), 245 abbreviations, a
    973-prefix table built from 48,453 real callsigns.
- **Field Console (browser, Web Serial):** dashboard with **training presets** — *Beginner Calls, CWOps
  Copy, Weak Copy, SOTA Hunter, Decoder Ready* — that "write existing firmware preferences over USB, then
  launch the matching mode"; a Device Health page (console-only: firmware, battery, storage, RX
  sensitivity, keying, with a recommended next action); a CW audio generator for decoder testing; TTY; flash.
- **Two engineering details worth noting:** (1) *Power guard:* "stale USB protocol sessions now expire
  after 10 minutes so idle shutdown can run" — the problem we have recorded and deferred as
  "heartbeat + lapse" (an abandoned protocol session blocks auto-power-off). (2) *Menu table repair:* a
  release shipped a blank menu because the menu enum, text table and navigation table fell out of step
  under conditional compilation; they fixed it with inferred lengths plus compile-time assertions.

**Reading of the above.** The fork is a **content-and-field-features product** (real callsign data, SOTA/POTA,
half-duplex keyer+decoder, decoder calibration) on a slimmed Pocket build. It does **not** add protocol
commands for training, and its "training control" is presets over the stock protocol. *An earlier
draft of this file guessed the fork might be filling a protocol gap; the notes do not support that.*

**Why it matters for the decision.**
1. **Their M32 support is shallow where an add-on could be deep:** decoded text only, no timing, no
   structured events, and (as far as seen) no use of the M32's own Generator/Echo/Koch loop.
   Everything in §3 items 3, 4, 8, 15 remains unserved for the M32 specifically.
2. **Two different bets.** OM0RX puts *content* (callsign pools, SOTA/POTA) on the device and *analytics*
   on the website. That supports a split for any add-on of ours: content-heavy generators belong in firmware
   where flash allows; scoring, scheduling and history belong on the host.
3. **The Field Console shows the stock protocol is enough to drive training workflows:** presets
   (`put config/...` plus menu launch) and start/stop need no firmware change. Options A/B can do the same.
3. **Support risk:** users of an unofficial build (and of a console that suggests downgrading from 9.1)
   may report problems to the official project.
4. **Competitive reality:** a hobby-scale add-on will not out-feature a ~40-module platform; it can be
   free, open, device-native, accessible, and tied to our curricula and protocol.

### 1b. Send-direction trainers: IW7DMH and OZ1THC (read in full, 2026-10-29)

*Added at Willi's request: the Phrases Trainer in particular, because a phrases trainer is a useful
complement to one that handles single words only. Method: the full text of both IW7DMH pages (new
and old release), and for OZ1THC the GitHub repository — README, release notes and the complete C#
source of v1.1 (2.0 is binary-only). Neither program was run. My first pass over these two (the
table and the "what the other add-ons add" bullet above) came from a truncated page fetch and the
README alone; it called IW7DMH's modes "not described", which was wrong.*

**The shared design — the opposite of tegmento.** The host chooses a target text and *shows or plays*
it; the user answers by **keying it on the paddle**; the M32 decodes the keying and the host compares
the decoded stream with the target. These are *sending* trainers (keying accuracy, memory), not copy
trainers. Neither uses `cw/play` or any other protocol command — they only read the raw character
stream (*Serial Output*), which is why they work on firmware back to 2.2 and in CW Keyer mode.

**IW7DMH CW Trainer (Qt, closed source).** Two generations: an old Windows-only release (Com Port +
Open; free *Training* mode with a timer and letter / number / sign counters; an *Attempt* mode with
generated groups and a red error counter) and a 2021 rewrite ("multi-platform, multi-skin", Windows
installer, Ubuntu zips, Raspberry Pi in test). Setup is callsign, serial port, skin.
- **Koch Trainer:** lesson picker (letters, numbers, prosigns, ordered top-to-bottom and
  left-to-right, with select-all checks) and generator options *lines*, *groups per line*, *group
  length* (0 = random). Wrong characters turn red; correct ones blue/black or cyan/white per skin.
- **Random Words** (500 most common English words), **Random Calls** (from the Super Check Partial
  file), **Q-Code Abbreviations** (from the author's own ham manual): one item at a time; a correct
  item advances to a new line, a wrong one is re-shown on the same line, up to a configurable
  *maximum number of repeats* (0 = until correct).
- **File** and **Cut/Paste** modes: same engine, text from a file or the clipboard.
- **Mechanics.** `*` and `<err>` in the stream are "go back" (the error counter is *not* decreased).
  A *Monitor* dialog shows the received stream next to the program's text; *View Log* shows the
  unfiltered stream; the *Report* dialog gives accuracy, CPM and WPM (the last two only if the timer
  ran). Author's stated philosophy: "rhythm and precision, not speed"; the timer is "for convenience".
- **Not there:** adaptivity, spaced repetition, persistent history, response times, phrases.

**OZ1THC Phrases Trainer (C# / WinForms, NAudio; source for 1.0 / 1.1).**
- **Flow.** The user picks a file from a `Phrases/` folder of `.txt` files; the program draws a random
  line, **plays it itself** from its own tone generator (WPM, pitch, volume, Farnsworth for letters
  and for words, an optional white-noise bed, an optional "voice"), shows it or hides it (a checkbox),
  and the user **keys the phrase back** on the M32. A decoded `=` ends the answer; six dits (`*`)
  delete the last word. The words are joined and compared with the target after lower-casing and
  trimming. Correct → the program sounds "R" in CW and prints "- OK"; wrong → it sounds "W" and
  prints "- Error -" with what was sent. After a configurable interval the next phrase starts. Counters:
  total / correct / wrong.
- **Content.** Shipped files are one per phrase length, 2 to 8 words (`phrases_2_words.txt` …
  `phrases_8_words.txt`), plus a list of US state codes; every line ends in a ` =` terminator. The
  phrases are ordinary English sentences and sayings with punctuation — no QSO phrasing, no
  abbreviations or Q-codes, no relation to a Koch lesson. Version 2.0 (binary only, per its README)
  adds a one-letter reveal, one-word delete, repeating the played sound and the sent phrase, an
  optional `=`, and AD0WE's "ninja" text files. The repo carries no licence, so the corpus and code
  are ideas only, not ours to copy.
- **What the source shows about its limits.** Scoring is exact string equality on the whole phrase
  (pass/fail; no word alignment, no partial credit); the phrase is drawn with
  `rand.Next(0, lines.Count - 1)`, which can never pick the last line; the audio is the PC's, so the
  speed set on the PC is independent of the M32's; no timing, no history, no adaptation.

**What this adds to the picture.**
1. **Two directions, not one.** *Copy* (hear → type; tegmento, Head Copy) and *send/keyed-echo*
   (hear or see → key; IW7DMH, OZ1THC, the TX Trainer of Morse Trainer Pro). The M32's own Echo
   Trainer is a keyed-echo trainer, but its unit is **one word or group of characters** (manual,
   Echo Trainer; sub-menus Random, CW Abbrevs, English Words, Call Signs, Mixed, File Player,
   Practice Set) — it never presents a multi-word phrase as a single unit. **Phrase echo is a real
   gap on the device.**
2. **What phrases train that single words do not:** word boundaries and inter-word gaps at speed,
   short-term memory ("head copy"), the idioms of a QSO, rhythm across words, prosigns in context.
   This is why Head Copy's curriculum is phrase-heavy. For the M32 the corpus must be **CW-idiomatic**
   — QSO fragments, Q-codes in context, contest exchanges, with prosigns — not generic English
   (CLAUDE.md §1: correct CW conventions are a product requirement).
3. **Nobody filters phrases by the learner's Koch lesson.** A phrase whose characters are all within
   the learned set is a natural fit for the M32's Koch trainer and is not offered by any tool seen.
4. **A phrase length ladder** (OZ1THC's 2 → 8 words) is an easy, understandable difficulty axis.
5. **Scoring multi-word answers needs more than equality:** word-level alignment, per-word and
   per-character error attribution, partial credit, and mastery tracked per word and per character.
6. **Stream facts both tools depend on** (from `m32_v6.ino`): the raw stream merges three origins
   (keyed, decoded, generated) into one text stream, selected by *Serial Output*: Nothing, Keyed,
   Decoded, Keyed+Decoded, Generated, All. Text sounded by `cw/play` goes through
   `displayGeneratedMorse` → `SerialOutMorse(…, 0b100)`, so by that code path it appears in the
   stream **only** with *Generated* or *All* — a keyed-answer trainer would want *Keyed* so its own
   prompt does not pollute the answer (*to be confirmed on the bench*). The eight-dot error sign
   arrives as `<err>` (the decoder's code table maps it to the `<err>` prosign; IW7DMH documents the
   tag), while `*` is what the decoder emits for a sequence it cannot decode (`m32_v6.ino:3584`,
   `:3595`) — OZ1THC's "six dits = `*`" is such a sequence. Both tools treat `*` as their "delete"
   signal and IW7DMH also `<err>`; a new tool should accept both (*bench to confirm*).

---

## 2. General CW trainers (not tied to the M32)

| Program | Kind | What it offers |
|---|---|---|
| **LCWO.net** | Web, free, accounts | Koch course; speed practice in four modes (code groups, plain text, callsigns, words); WAE QTC training; **personal statistics + public high scores**; MP3 export for offline practice; text-to-CW without login; many UI languages |
| **G4FON Koch trainer** and clones (Koch Morse Trainer Pro, Wolphi's Morse Trainer — *third-party mirrors, low reliability*) | Desktop / Android | Random groups of 5 from a chosen character subset; character speed vs. effective speed (Farnsworth); play user-supplied text files; callsign lists |
| **Morse Machine** | Windows | Adaptation of Ward Cunningham's Morse Code Teaching Machine; Koch; ≥ 20 WpM character speed so sounds, not dot-dash patterns, are learned |
| **RufzXP** | Windows | 50 real callsigns per session, typed on keyboard; **speed adapts** up/down per answer; replay costs 50 %; load your own callbase (e.g. contest master files); score from speed and accuracy |
| **Morse Runner** (VE3NEA) | Windows | Contest simulator: pile-up / single-call / etc. modes; QRM, QRN, QSB, flutter, "LIDs"; keyboard logging practice. Descendants: **Morse Walker** (web), **CW Runner QT** (iPad, paid) |
| **Head Copy CW Trainer** | Flutter (Android/iOS/web), MIT | One adaptive flow: *recognition → copy-behind → on-air*. 215 curriculum items (QSO phrases, ~110 words, callsigns incl. portable, user's own call early); Leitner boxes; mastery = ≥ 3 recent reps, ≥ 90 % rolling accuracy, median recognition ≤ 550 ms; **character speed fixed at 20 WpM, only Farnsworth spacing moves (8–20)**; silent gap before the prompt grows 0.4 → ~2 s; tracks confusable characters; time measured at first keystroke |
| **morsecode.world ICR / Adaptive ICR** | Web | Instant Character Recognition: pick problem characters, add more; adaptive version times responses and focuses weak characters; character sets from CWops, LICW, LCWO, Koch |
| **Morse Code Ninja** (AD0WE) | Web | Large collection: instant character *and word* recognition, callsigns, QSOs, sentences, speed-racing (details not verified) |
| **Morse Trainer Pro (OM0RX)** | Web + iOS, freemium | ~40 trainers (see §1a): adaptive engine, Echo and Head Copy labs, callsigns, Morse Runner clone, weekly planner, Premium "Training Intelligence" (confusion pairs, hesitation, replays), cross-device sync; has Morserino/WinKeyer/VBand/Elecraft TX trainers |
| **CW Academy (CWops)** | Curriculum + tools | Beginner → advanced courses; Morse Trainer for beginners; RufzXP and Morse Runner for intermediate/advanced; ≥ 60 min/day, 6 days/week; practice audio; weekly CWT contests; head-copy files |
| **LICW curricula** | Club curriculum | Three "carousels" (BC1: 18 chars in 6 lessons, 12 WpM / 8 FWPM; BC2: 44 chars in 9 lessons incl. numbers, prosigns, BK, 10 FWPM); characters ordered by QSO frequency; start at any carousel |
| **Morse Mania** (iOS) | App | Level order easiest → hardest; settings for review time, progress speed, time pressure, lives; jump back/skip |
| **MorseKit** (iOS) | App | SM-2 spaced repetition, tracks weak spots (per its store listing) |
| **Morse Code Trainer & Keyboard** (iOS) | App | Send mode with **live dit/dah timing feedback**; paid tier adds games and progress dashboard |
| **K3NG keyer** training menu | Firmware | Random groups, words, callsigns — i.e. the same as our on-device generator |
| **Wordsworth** (K1IG, QST May 2017), **Wisco CW Trainer** | Method / web | Word-based learning; frequency-ranked word lists (top 500 → ranks 1001–5000) |

Notes: no source I found reviews fist-quality analysis for paddle sending in any depth (Morse Trainer
Pro's TX trainers might do it; undocumented) — it looks like an under-served niche, and one where the
M32 (which sees the key line) is better placed than any PC program.

---

## 3. Feature catalogue — what exists, and where the M32 stands

"M32 today" = what the firmware/Config Tool already does (checked against `devdocs/` and the protocol doc).

| # | Capability | Who has it | M32 today |
|---|---|---|---|
| 1 | Koch progression with accuracy gate | LCWO, G4FON, tegmento, CW Academy | Koch trainer on device; adaptive word probability; gate is manual |
| 2 | Random groups / words / callsigns / abbreviations | all | On device (Generator, Echo, Koch) |
| 3 | **Typed-copy scoring** (keyboard in, diff out) | LCWO, RufzXP, tegmento, Morse Runner | **Not possible on device** (no keyboard) — the add-on's natural job |
| 4 | **Response-time measurement** (first keystroke) | Head Copy, ICR, RufzXP, Morse Trainer Pro ("hesitation") | Not available; Echo Trainer measures nothing |
| 5 | **Adaptive speed** per answer | RufzXP, Head Copy | Echo Trainer has *Speed Adapt* (±1 WpM per word) |
| 6 | **Spaced repetition / mastery tracking** (Leitner, SM-2) | Head Copy, MorseKit; Morse Trainer Pro (adaptive engine) | Only word-probability bumps in `Koch`; no per-character mastery |
| 7 | **Confusable-character tracking** | Head Copy, Morse Trainer Pro (Premium) | Pocket practice stats count per-character errors; no pair analysis |
| 8 | Copy-behind (delay before prompt) | Head Copy | No |
| 9 | Callsign drills with real-call databases | RufzXP, Morse Runner | `getRandomCall()` + prefix list; no real callbase (the OM0RX fork has 253,774 real calls on the device, §1a) |
| 10 | Pile-up / contest simulation with QRM/QSB | Morse Runner, CW Runner | Pocket has *Fight the Pileup* (game) |
| 11 | QSO simulation / bot | tegmento, qsobot.online, qsosrv | On-device **QSO Bot** (both variants) |
| 12 | Persistent, multi-device statistics, trends | LCWO, MorseKit, Morse Trainer Pro (synced, accounts) | Pocket: `/stats.jsonl` + Koch tab; browser-only in tegmento |
| 13 | Daily plan / curriculum / lesson scheduling | CW Academy, LICW, Head Copy, Morse Trainer Pro (weekly planner, path questionnaire) | Snapshots (+ bundle idea in `devdocs/snapshot-bundles/`) |
| 14 | Text-to-CW / MP3 export, play own texts | LCWO, G4FON | File Player + file upload |
| 15 | **Sending-quality feedback** (timing of dit/dah/gaps, weight, rhythm) | one iOS app (live timing); Morse Trainer Pro has timing analysis only for its virtual key — on the Morserino it reads decoded text (§1a) | **Not done** — the keyer sees every element |
| 16 | Speech / blind-user support | tegmento (planned) | Pocket Accessibility Edition (device speaks UI) |
| 17 | Device configuration | tegmento | Config Tool + iPhone app (ours) |
| 18 | **Phrase / multi-word items** (2–8 words, QSO fragments, sentences) as one unit | OZ1THC Phrases Trainer (English sentences), IW7DMH (File / Cut-Paste text), Head Copy (QSO phrases in its curriculum) | **Gap.** Echo Trainer's unit is one word or group; File Player plays phrases but no tool presents them as one echo unit. No Koch-filtered phrases anywhere (§1b) |
| 19 | **Keyed-echo scoring** (paddle in, decoded text diffed against a host-chosen target) | IW7DMH, OZ1THC, Morse Trainer Pro (TX Trainer); on the device itself the Echo Trainer, for one word at a time | Device: single words. Host tools read the decoded stream and need no firmware support |

**Reusable ideas with clear provenance.** Head Copy is MIT-licensed, so its *algorithms* (Leitner
promotion, mastery thresholds 90 % / 550 ms, Farnsworth auto-tightening, growing copy-behind gap) can
be adopted with attribution; everything else in §2 is idea-level only (no code to copy). tegmento is
Apache-2.0 (could be forked or contributed to).

---

## 4. What the M32 side gives an add-on (and what is missing)

Read from `Documentation/Protocol Description/M32 Protocol.md` (protocol 1.5) and the firmware.

**Available now:**
- Raw character stream of keyed and generated characters (*Serial Output*), USB and BLE.
- `PUT cw/play`, `cw/repeat`, `cw/stop`, memories — the host can make the device sound arbitrary
  text (only in CW Keyer / Morse Trx mode, at the device's speed and spacing settings).
- Files (`put file/…`), snapshots, Koch lessons, Practice Set, custom characters, player identity.
- `GET stats/log` (Pocket) and `GET game/scores`; `GET capabilities` for feature probing.
- Transport seam already isolated in the Config Tool (`openTransport()`), so USB, BLE and the iOS
  app share one code base.

**Gaps I found for a *training* add-on:**
1. **No structured trainer events.** `echoTrainerEval()` (`m32_v6.ino:3404`) decides OK/ERR and
   updates Koch weights, but the verdict reaches the host only as the text "OK"/"ERR" in the raw
   character stream (via `displayGeneratedMorse` → `SerialOutMorse`). The expected word and the user's
   response are in the stream too, but there is no delimiting object and no timestamp — a host has to
   infer both. Cf. the `<answer>, ERR` trap in the memory note on `gameCharBuffer`.
2. **No device-side timing.** Host timestamps at arrival are good enough for 550 ms-class thresholds
   (USB/BLE jitter is milliseconds) but not for element-level sending analysis (item 15), which would
   need the keyer to emit dit/dah/gap durations.
3. **Serial Output per snapshot** is a setup friction (tegmento documents it). Probably fixable by the
   add-on setting it through `PUT config` on connect — *to be verified*.
4. **Device-driven only.** In Generator/Echo/Koch modes the *device* chooses the text. A host-driven
   mode (host picks the item, device sounds it via `cw/play`, host scores the typed answer) works today
   in Keyer mode, but then the device's own Koch/Echo machinery is bypassed.
5. **Stats are Pocket-only** (`CONFIG_PRACTICE_STATS`); classic M32 would rely on host-side storage.
6. **No phrase unit on the device.** The Echo Trainer echoes one word or group at a time (§1b). A
   phrase trainer therefore has to live on the host (host picks the phrase, `cw/play` sounds it,
   the answer comes either from a keyboard or from the keyed/decoded stream). It needs `cw/done`
   (to know when playback ended) but no other new device feature.
7. **The raw stream does not label its origin.** Keyed, decoded and generated text arrive merged;
   a keyed-answer trainer must therefore require *Serial Output = Keyed* (whether `PUT config` may
   set it per session is still to be verified, as in gap 3). Text longer than 255 characters is not
   an issue for phrases (`cw/play` index is `uint8_t`, `m32_v6.ino:151`).

An add-on could even run with **no device audio at all** (host synthesises CW in Web Audio) and use
the M32 only as paddle/key input — that makes it usable as a pre-purchase trainer, but weakens the
"M32 add-on" identity. Worth deciding explicitly.

---

## 5. Where an add-on adds value, and where it would duplicate

**Clear value (the device cannot do it):** typed-copy scoring (3), response-time (4), mastery model /
spaced repetition (6, 7), copy-behind (8), durable cross-device history (12), a lesson plan with
daily targets (13), real-callsign drills (9), sending-quality feedback (15).

**Would duplicate what exists:** generators (2), QSO bot (11), pile-up (10), Koch gating by hand (1) —
unless the add-on *drives* those through the protocol rather than re-implementing them.

**Phrases (items 18–19) are the least crowded ground.** The two tools that touch them are
send-direction only, exact-match, Windows-desktop, unlicensed or closed, last released 2020–21, with
English-sentence content and no adaptivity. Nobody offers a CW-idiomatic or Koch-filtered phrase
corpus, a phrase-length ladder tied to mastery, or word-level scoring. That is clear value, and it
needs only one device-side addition (`cw/done`), because the keyed answer is already in the stream.

**Overlap with tegmento** is large for items 1–3 and 13 (CW School Graz). It is linked from
morserino.info, and its author (Christof Dallermassl, OE6CHD, GitHub `cdaller`) appears to be the
Christoph Daller whom CLAUDE.md §8 names as co-designer of the protocol text stream — *identity
inferred from the name, to be confirmed*. Either way, a competing app should be a deliberate choice,
not an accident.

**Overlap with Morse Trainer Pro** is wider but shallower on the M32 side (hands-on, §1a): it covers
items 3–7, 12, 13 generically for any input device, with Premium paywalls for the analytics, and it is
closed source. For the M32 it offers a keying trainer fed by decoded text, and a config console that
mirrors ours. Not seen: Koch, the M32's own Generator/Echo/Koch loop as a data source, element timing,
the accessibility edition, EN+DE curriculum integration. What its fork firmware changes is unknown.

---

## 6. Ways to build it (costed at the level of this research)

| Option | What | For | Against |
|---|---|---|---|
| **A. New "Train" tab in the Config Tool** | Training engine + UI inside `Software/Utilities/m32_config_tool.html` | One code base; reaches the iPhone app automatically (BLE); versioned with firmware | Tool is already ~2 700 lines in one HTML file; training state/UI would bloat it |
| **B. Separate web app (PWA) sharing the transport layer** | New page in `Software/Utilities/` (or own repo); engine as plain JS module with host tests | Clean separation; offline-capable; Web Serial + Web Bluetooth (Android Chrome); engine testable like `Software/tests/protocol/` | A second app to maintain; iOS needs the app shell (`bridge.js` seam) |
| **C. Join forces with tegmento** | Contribute engine + protocol updates upstream (Apache-2.0) | No duplicate; existing users; his CW-school context | Depends on his roadmap/availability; our protocol/a11y standards need agreement |
| **D. Native app (Flutter, like Head Copy)** | Android + iOS + desktop | Best mobile story, background audio | Largest effort; separate release channel; protocol client rewritten |
| **E. Firmware only** | More of the engine on the device | No host needed | Display too small for trends; NVS budget (CLAUDE.md §4); Pocket-only for stats |
| **F. Don't build; document and bridge** | Point users at tegmento and Morse Trainer Pro; spend the effort on protocol 1.6 (trainer events, timing) so *any* add-on can do better | Lowest cost; helps every tool; keeps the firmware as the product | No M32-branded training experience; depends on third parties adopting the events |

Possible protocol work for any of A–D (would be protocol 1.6, additive): a structured `trainer`
event (mode, expected, received, verdict, ms from prompt end to first key), optional element timing in
the keyer, and `PUT config` guidance for *Serial Output*. Each needs manuals (EN+DE), the a11y check
(§8), and the harness in `Software/tests/protocol/`.

---

## 7. Decisions for Willi

1. **Build a comprehensive trainer at all?** The case for: items 8 and 15 (copy-behind, sending
   quality) and a free, open, device-native, accessible version of 3–7 and 12–13 are not covered by the
   M32 add-ons that are documented. The case against: heavy overlap with tegmento and — more
   so than I first thought — with Morse Trainer Pro, plus what the Pocket already does on its own.
   Morse Trainer Pro has now been tried hands-on (§1a); tegmento has not.
2. **Which option (A–F)?** And is collaboration with OE6CHD (C) a precondition or an alternative?
3. **Platforms:** desktop browsers only (Web Serial), or also iPhone (existing app shell + BLE) and
   Android (Web Bluetooth)?
4. **Scope of a first version:** e.g. typed-copy trainer with per-character mastery + response time +
   persistent history for Koch lessons; callsign drills second; sending-quality analysis later (needs firmware).
5. **Does the add-on need to work without a device** (host-side audio), or is the M32 mandatory?
6. **Accessibility:** is blind-user operation (speech output, keyboard-only) a v1 requirement? tegmento's
   own README lists it as open.
7. **Device-side changes:** accept a protocol 1.6 with a `trainer` event as a prerequisite?
8. **Contact OM0RX?** What 8.2-OM0RX changes is now known (§1a). Remaining questions: source availability
   (GPLv3), the "Update available" wording that offers 8.2 over 9.1, and whether content such as the
   callsign pools could be shared with upstream or an add-on. A friendly first approach — to be made by
   Willi, not by me.
9. **Phrases (Willi, 2026-10-29: "very useful" — include).** Open sub-questions, costed in
   `LEAN_V1_SCOPE.md` §7: the answer modality (typed on the keyboard, keyed on the paddle, or
   both); the corpus (our own CW-idiomatic, Koch-filterable set — the OZ1THC lists are
   unlicensed English sentences and not ours to copy); and the ladder (phrase length 2 → 8 words).

---

## 8. Spin-offs for the firmware (independent of the add-on decision)

Ideas the OM0RX notes suggest; none is started, none is decided.
- **Idle expiry for abandoned protocol sessions** (their 10-minute "power guard") — our deferred
  "heartbeat + lapse" item; a data point for the design.
- **Compile-time guard for the menu tables.** `menuText[menuN]` and `menuNav[menuN][5]`
  (`MorseMenu.cpp:87`, `:233`) are fixed-size arrays, so *too few* initializers are silently
  zero-filled (a null label or dead navigation row); too many fail to compile. Our preference arrays
  already have `static_assert`s (`MorsePreferences.cpp:129`, `:629`, `:653`); the menu tables have none.
  Not verified whether a conditional-compilation mismatch can occur today.
- **Callsign-trainer features** to weigh for the generator / QSO Bot: real-callsign pools, prefix drill,
  similar-call mode, a missed-call review queue (a cheap, device-side form of spaced repetition),
  SOTA/POTA exchanges. Flash and NVS budgets (CLAUDE.md §4) decide what fits.

---

## Sources

- Morserino software overview: <https://www.morserino.info/>
- M32 CW Trainer (OE6CHD): <https://tegmento.org/> · <https://github.com/cdaller/morserino32-trainer>
- IW7DMH trainer: <https://iw7dmh.jimdofree.com/other-projects/cw-trainer-for-morserino-32/> (new
  release; full text read 2026-10-29) · old release:
  <https://iw7dmh.jimdofree.com/other-projects/cw-trainer-for-morserino-32/cw-trainer-for-morserino-32-old-release/>
  (the new page's 12 screenshots are uncaptioned and were not viewed; nothing was downloaded)
- Phrases Trainer (OZ1THC): <https://github.com/Tommy-de-oz1thc/Morserino-32-Phrases-trainer>
  (branch `Morserino`; README, release notes and C# source of v1.1 read through the GitHub API on
  2026-10-29; nothing run or downloaded as an executable)
- QSO Bot (M0STQ): <https://qsobot.online/> · qsosrv: <https://github.com/pavian57/qsosrv>
- m32chat: <https://git.rustysoft.de/martin/m32chat/> (README seen only via search snippet; fetch blocked)
- Morse Trainer Pro (OM0RX): <https://morsetrainerpro.com/> (front page only; sub-pages 404) ·
  iOS app: <https://apps.apple.com/us/app/morse-trainer-pro/id6756198226> ·
  namesake on SourceForge (unrelated?): <https://sourceforge.net/projects/morse-trainer-pro/> ·
  M32 Pocket OM0RX Field Console and its Firmware Notes tab: saved by Willi on 2026-10-28 (local file, not republished)
- LCWO: <https://lcwo.net/> · Head Copy CW Trainer: <https://github.com/jhnhnsn/headcopycw>
- morsecode.world: <https://morsecode.world/international/> · Morse Machine: <https://www.g4ilo.com/morse-machine.html>
- CW Academy: <https://cwops.org/cw-resources/> · LICW: <https://longislandcwclub.org/curricula-introduction/>
  · LICW Morserino guide: <https://longislandcwclub.org/wp-content/uploads/2025/03/Morserino-BC-WITH-KOCH-TRAINER-03-2023.pdf>
- RufzXP / Morse Runner write-ups: <https://k0pir.us/two-excellent-cw-training-tools-memory-training/> · <https://swling.com/blog/2020/07/upping-my-cw-pile-up-game-with-morse-runner>
- K3NG training menu: <https://github.com/k3ng/k3ng_cw_keyer/wiki/495-CW-Training-Functionality>
- Morse Mania / MorseKit (App Store listings): <https://apps.apple.com/us/app/morse-mania-learn-morse-code/id1511042196>
- Firmware updater: <https://github.com/joewittmer/Morserino-32-Firmware-Updater>
