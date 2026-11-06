# M3 bench — the M32 Trainer page on real devices

*Written 2026-11-01 with the M3 page (branch `training-addon`). Willi runs this; Claude cannot
bench. Run it on an **M32 Pocket** and a **classic M32**, in **Chrome** and in **Edge**. The firmware
is the M1 build already on both devices (protocol 1.6); M3 changes no firmware.*

What this bench is for: everything the Node tests and the simulated M32 cannot cover — the real
Web Serial transport (`openWebSerial()`, M2), the timing feel of "Listen…" → "Type…" against the
sound, and the device-side reactions (paddle, menu, cable, restart, tab close).

## 0. Open the page

The page loads two side files, so serve the folder instead of opening the file. From
`~/Documents/GitHub/Morserino-32-training-addon/Software`:

```
python3 -m http.server 8732 --bind 127.0.0.1
```

Then open `http://localhost:8732/Utilities/m32_trainer.html`. Close the Config Tool and any
serial monitor first (one program per port).

**Keep the browser console open** (Cmd+Opt+J in Chrome, Cmd+Opt+I → Console in Edge): the
page logs every line it sends (`→`) and receives (`←`) and every link state change. When a step
fails, copy the console from a little before the failure.

*Without a device:* `Software/tests/trainer/fake_m32_browser.js` is a simulated M32 — paste it into
the console before pressing Connect (see its header).

## 1. Connect

| # | Do | Expect |
|---|---|---|
| 1.1 | Connect, pick the M32's port | Badge "Connected · CW Keyer"; Device, Firmware (protocol 1.6), Koch lesson, Speed filled in; the M32 is now in the CW Keyer |
| 1.2 | Start the M32 in some other mode first (e.g. CW Generator), then Connect | The M32 leaves that mode and enters the CW Keyer by itself |
| 1.3 | Your character table | One row per character of your lesson, all in box 1 on a first visit |

## 2. Practise (keyboard only — do not touch the mouse)

| # | Do | Expect |
|---|---|---|
| 2.1 | Enter | The M32 sounds a group; "Listen…"; the cursor is in the answer field |
| 2.2 | Wait for the end | "Type what you heard" appears **as the last element ends** — note if it feels late or early (M1 measured 0–80 ms Pocket, 15–155 ms classic) |
| 2.3 | Type it, Enter | "Right."; the table row counts up |
| 2.4 | Type a wrong answer, Enter | The group shown with the wrong character marked; the M32 plays it **once more**; Enter goes on. About four groups later the same group comes back |
| 2.5 | Ctrl+Enter (Cmd+Enter on a Mac) while answering | The group sounds again; the button says "(0 left)"; after Check: "You used a replay, so this one does not move a box" |
| 2.6 | Enter while it is still sounding | "Still sounding — answer when it has finished"; nothing is scored |
| 2.7 | Type while it is sounding, then Enter after the end | Accepted (copying while listening is normal) |
| 2.8 | Esc | "Paused"; Enter goes on |
| 2.9 | Group size: pin 3, then Mixed | Groups of 3 (the console shows `→ put cw/play/` with three characters); Mixed starts with single characters while your boxes are low |
| 2.10 | ~30 groups | Weak characters come up more often; the newest character of the lesson especially |

## 3. The device side

| # | Do | Expect |
|---|---|---|
| 3.1 | Touch a paddle while a group sounds | "A paddle stopped the M32 — playing it again"; the group plays again from the start |
| 3.2 | Long-press to leave the CW Keyer | The page pauses: "The M32 left the CW Keyer. Press Enter to go back to it."; Enter → the M32 is back in the CW Keyer and practice goes on |
| 3.2b | Long-press out of the CW Keyer, then start the **CW Keyer on the M32 yourself** | The page says "Back in the CW Keyer. Press Enter to go on." and the badge is "Connected · CW Keyer" again; Enter plays |
| 3.2c | Long-press out, start a **different** mode on the M32 (e.g. CW Generator), then Enter on the page | The M32 leaves that mode and goes back into the CW Keyer by itself |
| 3.3 | **Pocket:** unplug the USB cable mid-group, wait 5 s, plug it back | "Cable lost", then by itself "Connected · CW Keyer" and "Back again. Press Enter to go on." (the M2 `reopen()` path — **the first real test of it**) |
| 3.4 | **Classic:** restart the M32 (power switch or reset) while connected, then press Enter for the next group | The classic's port stays open, so the page notices only when a command goes unanswered: about 3 s after Enter it re-handshakes, puts the M32 back in the CW Keyer and says "Back again" (the M2 `resync` path); Enter then plays |
| 3.5 | Close the tab while connected | The M32 leaves protocol mode; with a time-out set, it powers off on time again (the `pagehide` goodbye) |
| 3.6 | Disconnect button | Same as 3.5, without closing the tab |

## 4. History and lessons

| # | Do | Expect |
|---|---|---|
| 4.1 | Reload the page, Connect | The table shows your earlier work (IndexedDB) |
| 4.2 | Export… then, in **the other browser**, Import… that file | The same table; importing it twice adds nothing ("… were already here") |
| 4.3 | Get every character to box 3 (or set a small lesson, e.g. 2, to get there quickly) | A green offer "Go to lesson n+1 (changes the lesson stored on your M32)"; it never moves by itself; clicking it changes the M32's lesson and adds the new row |
| 4.4 | Set a lesson ≥ 43 on the M32, reconnect, pin group size 5 | Under the keys: "Type a prosign in angle brackets…"; groups with a prosign sound it as **one** prosign (SK = ···−·−, not "S K"), and the M32's display shows it as `<sk>` |
| 4.5 | Answer such a group typing `<sk>`, then one typing `[sk]` | Both "Right." (`\sk` too) |

## 5. Screen reader (quick look)

| # | Do | Expect |
|---|---|---|
| 5.1 | VoiceOver on (Cmd+F5), do 2.1–2.4 with Enter in the answer field | No speech while it sounds. "Type what you heard…" when the sound ends; "Right! …" at once; for a wrong answer the replay comes first, then "It was K M R. You typed K M T. Press Enter…" |
| 5.2 | Activate Next with VoiceOver (VO+Space) | VoiceOver reads the answer field, the sound starts about 1.5 s later, over nothing |

## 6. Words and phrases (M3b)

| # | Do | Expect |
|---|---|---|
| 6.1 | Practise: Words, at lesson 16 | Real words, only of your lesson's characters; the line under the controls says how many |
| 6.2 | Practise: Phrases, 2 words, From Mixed, at lesson 16 (the first lesson of the M32 sequence where all three kinds qualify at 2 words) | The *From* list shows counts and greys out empty kinds; items from each kind with ≥ 10 |
| 6.3 | Get a phrase partly wrong | The words you missed are marked; it plays once more; it comes back about 4 items later, while the settings still fit it |
| 6.3b | Groups: miss a few at size 5, set size 1, Next about 8 times; then set size 5 or Mixed | Only single characters at size 1; the missed groups come back after the change |
| 6.4 | From: QSO lines at lesson 38 or later | Real QSO pieces and filled templates; call signs, names and QTHs only of learned characters |
| 6.5 | 10 phrases of one length, nearly all right | "Ready for n+1?" with a button; it never changes by itself |
| 6.6 | Your own phrases: paste a few lines (some too long, some with unlearned characters), Use these | The count line; From: Your own phrases plays only the usable ones |
| 6.7 | Reload the page | Practise, words per phrase, From and your own phrases are as you left them |
| 6.8 | Practise: Phrases, 8 words, From Mixed, at lesson 5 or lower (from lesson 6, word chains fill any length) | "Nothing to practise with these settings yet — …"; nothing is sent to the M32 |

## 7. Keyed answers (M3c)

**Needs the new firmware (`PUT cw/practice`, `PUT cw/spacing`) on both devices.** Run on the Pocket and the classic. First, from
`Software/tests/protocol/`: `python3 trainer_events.py` (its steps 7b and 7c). Then on the page, Answer: Keyed on the paddle.

| # | Do | Expect |
|---|---|---|
| 7.1 | Connect with Serial Output = All and Key ext TX = CW Keyer only (check both in the Config Tool afterwards) | Works; only what you key appears in the answer field, never the prompt; both settings are unchanged afterwards |
| 7.2 | With a rig or a key-line tester attached: practise, keyed | The sound and the sidetone are normal; **the key line never closes**, neither for the prompt nor for your keying |
| 7.3 | Groups, size 3: key the group correctly | It appears live; about 1.5 s after the last character "Right!"; the box table does not move; the "Keyed" column does; about 2 s later the next item starts by itself |
| 7.3a | Key a group wrongly. Listen to the replay, then key it again, right | The field clears and says "Key it again"; what you key appears live; about 1.5 s after the last character "Second try: right.", and about 3 s later the next item starts by itself. The tally and the "Keyed" column count only the first answer. The item comes back after about 4 others |
| 7.3b | The same, but key it wrongly the second time too | "Second try: you keyed …"; one try only: more keying is ignored until the next item |
| 7.3c | After a wrong answer, press Enter during the second try, with nothing keyed. Then again, after keying a character or two | Nothing keyed: the next item starts at once. Something keyed: that try is marked, then the next item follows as usual |
| 7.3d | After a wrong answer, touch the paddle during the replay | It stops and plays again (up to 3 times), then "Key it again". What you keyed during the replay is not in the field |
| 7.3e | After a wrong answer, key nothing for 15 s | No third playing; about 3 s later the next item |
| 7.3f | Keyed, Next item = Automatically: answer several items with your hands on the paddle only | It runs without the keyboard, and the pauses feel right. **Say if 2 s and 3 s are too short or too long** |
| 7.3g | During the wait for the next item: press Enter. Another time: press Esc | Enter: it starts at once. Esc: "Paused"; it stays paused until Enter |
| 7.3h | During the wait: open a setting (a dropdown) | The next item does not start and take the focus; "Press Enter for the next one" |
| 7.3i | Next item = Press Enter, keyed. Then reload the page | It waits for Enter as before; the setting is still there after the reload |
| 7.3j | Typed answers, Next item as set above | Always wait for Enter; the Next item setting is hidden |
| 7.4 | Size 5: key 2 characters, pause 2 s, key the rest | Not cut off |
| 7.5 | Key too few characters and stop | Ends after the "Finish after" time (try 2 s and 10 s) |
| 7.6 | Key some, press Enter | Ends at once |
| 7.7 | Key some, then `=` (BT) | Ends at once, the `=` is not in the answer; on a QSO line that contains `=`, the `=` is simply part of the answer |
| 7.8 | Phrase: key 2 words, the error sign (8 dits) | The last word goes; the error sign again right after: the whole answer goes |
| 7.9 | An undecodable character instead of the error sign | The same as the error sign |
| 7.10 | **Strict:** key a group with a gap of about 8 dits inside it | Marked wrong, the answer shows the break |
| 7.11 | **Lenient:** the same group with the same gap, then with a very long gap | Right, both times |
| 7.12 | **Lenient:** a phrase with about 8 dits inside a word and 15 dits between the words | Right. With less than 12 dits between the words: wrong (run together) |
| 7.13 | Strict and Lenient, phrases: listen | The sounded word gaps are the same in both |
| 7.13a | Set InterWord Spc to 25 on the M32, connect, phrases: listen. Then disconnect and run the CW Generator | The trainer's word gaps are normal (7 dits), not long; InterWord Spc still says 25 and the Generator uses it. Set it back |
| 7.13b | Set Interchar Spc to 6, connect, a group: listen. Set it back | The characters are spaced out as you set them |
| 7.13c | M32 screen = Hide what is played: practise, typed and keyed, groups and a phrase. Watch the M32 | Nothing of the prompt or the replay appears, not even blank lines; what you key does appear. Switch to Show mid-session: the next prompt appears. The setting is still there after a reload |
| 7.13d | Hide what is played, then disconnect and play a keyer memory on the M32 | The memory appears on the screen again (the mode has ended) |
| 7.13e | Keyed, Next item = Automatically: keep keying right through the moment the next item starts, several times, also with the error sign | It stops and plays again, up to 3 times, then waits for your answer. **It never hangs on "Listen…"** (this was the glitch of 2026-11-05) |
| 7.14 | Touch the paddle while it sounds | It stops and plays again (up to 3 times); nothing you keyed then is in the answer |
| 7.15 | Key nothing | After 15 s it plays once more; after another 15 s "Paused: nothing was keyed." Enter goes on |
| 7.16 | Set the M32's Output Case to upper case; key a prosign and `<err>` | Read the same as in lower case |
| 7.17 | Very slow speed (about 8 WpM), Lenient, Finish after 2 s, a phrase with a long word gap | Not cut off between the words |
| 7.18 | VoiceOver: a keyed item, also a wrong one | Nothing spoken while it sounds; "Key what you heard." when the sound ends; nothing while you key; the result after that. Wrong: nothing during the replay, then what it was and "Key it again."; the second try's result after you key it. With Next item = Automatically, does speech run into the next prompt? (That is what Press Enter is for) |
| 7.19 | Disconnect, then key on the M32 in the CW Keyer | Normal word gaps and keying again (the mode has ended) |
| 7.20 | During a session, long-press to the menu, start the CW Keyer on the device, press Enter on the page, answer keyed. With the rig or tester attached | The page says it is back in the CW Keyer; prompts and answers behave as before; **the key line still never closes** |
| 7.21 | During a session, leave the Keyer on the device, then press Enter on the page | The page starts the CW Keyer itself; the same as 7.20 |
| 7.22 | Battery powered: during a session pull the USB cable. Key on the M32 (rig or tester attached): it does not key. Long-press to the menu, start the CW Keyer, key | Now the **key line closes** and word gaps are normal. Before leaving the Keyer it stays in practice mode: this is known, a restart clears it too |

## 8. Characters and speed (M4)

**No new firmware**: what is on both devices since 2026-11-05 (master `ec59fe4` content). Page version **2026-11-06**:
reload while holding Shift. Run on the Pocket and the classic. Speed and spacing never change anything stored on the M32:
check Interchar Spc, InterWord Spc and the speed in the Config Tool before and after.

| # | Do | Expect |
|---|---|---|
| 8.1 | Characters = Koch lesson, connect | "Koch lesson: n of m" as on the M32. With **Custom Chars** on the M32 it adds "(Custom Chars)", and the groups use only that set |
| 8.2 | On the M32 choose the LICW sequence, lesson 44 (BC2), connect, practise groups | The characters table lists 44 characters, prosigns as `<SK>` `<BK>`; groups use only those |
| 8.3 | On the M32 set a Practice Set (CW Generator → Practice Set), e.g. K M R, `<as>` and an É. On the page Characters = Practice Set | The facts show the set; groups use only it; no "Go to lesson" offer; É and `<as>` sound right and are read right typed. Back to Koch lesson: the lesson again |
| 8.4 | Clear the Practice Set on the M32, then choose Practice Set (or press Start with it) | "Your Practice Set is empty: choose its characters on the M32 (CW Generator, Practice Set)." Nothing plays |
| 8.5 | Effective speed 12 WpM with the M32 at 20, groups and a phrase | The facts say "20 WpM, effective 12". The characters sound at 20, the gaps between characters and words are clearly longer, both of them |
| 8.6 | During a session turn the speed knob (e.g. to 25), then go on | The facts follow ("25 WpM, effective 12"); the next item has characters at 25 and still effective 12 |
| 8.7 | Turn the knob to 10 with effective 12 still set | A note: effective 12 is not below 10, no extra spacing; normal gaps. Effective speed Off: normal gaps too |
| 8.8 | Effective 12, Answer: Keyed, Strict | Keyed answers are read as before: the longer sounded gaps change nothing about how your keying is read |
| 8.9 | Disconnect, then run the CW Generator | Normal spacing, as your preferences say; Config Tool shows Interchar Spc, InterWord Spc unchanged, speed as you last set it on the knob |
| 8.10 | Phrases, QSO lines, several with call signs | The calls look real: call-area digits as often as heard on the air (OE mostly OE3 and OE5, then OE1, hardly ever OE0; DL1 most often among the DLs); only learned characters |

| 8.11 | **Before flashing** (page 2026-11-06b or later, reload with Shift): Custom Chars from your player file that ends in `\sk`, lesson 43, connect | Koch lesson fact: "43 of 43 (Custom Chars) — left out, no Morse code: \\". Groups never ask for a `\` |
| 8.12 | Flash the firmware from master with the `getWord()` fix (both devices). Koch Sequence: turn away from Custom Chars and back onto it, then Koch Lesson 43 | The lesson line shows `43-43: <sk>`, never `\`. Connect the page: no "left out" note. The File Player plays the last `\sk` as one prosign |

| 8.13 | Page 2026-11-06c, Characters = Koch lesson, connect | Next to Characters: **Koch sequence** (as on the M32) and **Lesson** ("12 · W": the lesson and the character it adds), on your M32's values. Practice Set hides both |
| 8.14 | Choose another Lesson on the page, without leaving the CW Keyer; then look in the M32 menu (Koch → Select Lesson) | "Lesson n of m. Practising it from the next item."; the next groups use it; the M32 menu shows the same lesson afterwards |
| 8.15 | Choose another Koch sequence (e.g. LCWO), then M32 again | The Lesson list follows the sequence (its characters), the fact too; the M32's Koch Sequence is changed likewise |
| 8.16 | Choose Custom Chars with a player file loaded; then (optional) with no `player.txt` | With the file: "(Custom Chars)", the lesson list is that set. Without: "The M32 has no custom character set … so it stays on M32." and the picker goes back |

**Known limit (not a failure):** a Practice Set or custom set with *ch* plays and reads it as two letters, c and h.

## 9. Call signs and prefixes (M4b)

**No new firmware.** Page version **2026-11-06e** (reload with Shift). Before you start, note the M32's Length Calls,
Calls Region and Call Prefixes (CW Generator preferences) and set them to something other than the defaults,
e.g. 4 / Europe / Common only. Nothing in this section changes anything stored on the M32.

| # | Do | Expect |
|---|---|---|
| 9.0 | Characters = Koch lesson (or Practice Set): open Practise | Call signs and Prefixes are greyed out |
| 9.0a | Characters = **All characters**, connect, groups | Facts: "Characters: all 51"; no Koch sequence / Lesson pickers; the table lists all 51; groups use characters beyond your lesson; no "Go to lesson" offer |
| 9.1 | Practise = Call signs (first connect with this page version) | Length Calls, Calls Region, Call Prefixes appear and show the M32's values; Group size and From disappear. The note: "Call signs as the M32 makes them, with the settings beside …" |
| 9.2 | Change the three on the page; disconnect, reload, connect | The page keeps its own values (not the M32's again); the M32's settings are unchanged |
| 9.3 | Practise some calls, typed: right and wrong ones | Calls as the M32's CW Generator makes them (length, region, common prefixes), also with characters beyond your lesson. A miss: shown, played again, and back after a few items |
| 9.4 | Calls Region = VK/ZL | VK and ZL calls only (mostly VK), 2 or 3 suffix letters |
| 9.5 | Practise = Prefixes | Prefixes like OE3, DL1, K4 (base + area digit, no suffix); Length Calls is hidden; Region and Call Prefixes apply (VK/ZL: VK2, ZL1 …) |
| 9.6 | Answer = Keyed on the paddle, a few calls | Keyed answers work as for groups |
| 9.7 | After some calls and prefixes, look at "Your characters" and "Call signs and prefixes" | The character boxes and counts did **not** change; the new section shows calls / prefixes copied, right, by length, most missed characters. The session line says "Calls and prefixes this session: …" |
| 9.8 | Back to Groups | All-characters groups as before; a missed call waits until you choose Call signs again |
| 9.9 | While on Call signs, switch Characters to Koch lesson | Practise goes back to Groups: "Call signs and prefixes need Characters = All characters. Back to groups. Practising your Koch lesson …" |

## Results

*(fill in: device, browser, step, pass/fail, notes)*

| Date | Device | Browser | Step | Result | Notes |
|---|---|---|---|---|---|
| 2026-11-01 | Pocket | Chrome | 2.9 | fail → pass | First run: every group was one character whatever the size. Not reproducible in the simulator; after a hard reload (and the console trace) groups have the length set. Most likely a stale cached page. |
| 2026-11-01 | Pocket | Chrome | 3.2 | fail → pass | First run: re-entering the CW Keyer on the M32 left the "left the CW Keyer" message; the page then showed "Not connected" and Connect said the port was already open (the M32's Goodbye left the port open). Fixed in the page; re-run passes. What made the M32 end the session was not captured. |
| 2026-11-01 | Pocket | Chrome | 3.2b | pass | Starting the CW Keyer on the M32 yourself is picked up |
| 2026-11-01 | Pocket | Chrome, Firefox | §1–§4 (all) | pass | Full run after 2ad98fa. Edge not available; Firefox used instead (it has Web Serial now) |
| 2026-11-01 | Pocket | — | §5 VoiceOver | not run | |
| 2026-11-01 | Classic | — | — | flashed | `heltec_wifi_lora_32_V2` from 2ad98fa (M1 firmware, protocol 1.6), ready for the run |
| 2026-11-01 | Classic | Chrome | §1–§4 (all, incl. 3.4 restart) | pass | One note: the key hint said Ctrl+Enter, which is Cmd+Enter on a Mac (both always worked); the hint now names Cmd on a Mac |
| 2026-11-02 | Pocket | — | — | flashed | `pocketwroom` (standard edition) from a865c21, firmware only, ready for the §6 run |
| 2026-11-02 | Pocket | not noted | §1–§4 | pass | Note on 2.9: long groups after setting size 1. In the simulator these were retries of groups missed earlier; size itself held. Missed items now wait until they fit (6.3b) |
| 2026-11-02 | Pocket | not noted | 5.1 | fail → re-test | VoiceOver talked over the Morse on Next. Now speech comes only around the sound, in the page's `#say` region; re-test owed |
| 2026-11-03 | Pocket | not noted | 5.1, 5.2, 6.1–6.8 incl. 6.3b | pass | Re-test on page version 2026-11-02, reported by Willi as all passed. Classic not reported for §6; Edge still not run |
| 2026-11-04 | Pocket | — | — | flashed | `pocketwroom` (standard edition) from `protocol-1.6-practice` 082af4b (current master + 1.6 firmware), firmware only, ready for §7 |
| 2026-11-04 | Classic | — | — | flashed | `heltec_wifi_lora_32_V2` from `protocol-1.6-practice` 082af4b, firmware only, ready for §7 and the open §6 |
| 2026-11-05 | Pocket | — | — | flashed | `pocketwroom` (standard edition) from `protocol-1.6-practice` 6b5f7e8 (adds `PUT cw/display`), firmware only |
| 2026-11-05 | Classic | — | — | flashed | `heltec_wifi_lora_32_V2` from `protocol-1.6-practice` 6b5f7e8, firmware only |
| 2026-11-05 | Pocket | — | `trainer_events.py` | pass | 46 passed, 0 failed, on 6b5f7e8 with the script at bad9eba. First full run of 7b and 7c: `cw/practice`, `cw/spacing`, `cw/display`, leaving the Keyer ends practice |
| 2026-11-05 | Classic | — | `trainer_events.py` | pass | 46 passed, 0 failed, on 6b5f7e8 with the script at bad9eba |
| 2026-11-05 | Pocket, Classic | not noted | §7 (all, incl. 7.13c–e) | pass | Page version 2026-11-05b on 6b5f7e8, reported by Willi as all passed, on both devices. Includes the key line staying open in 7.2, 7.20 and 7.22, and the M3c choices and the Hide default signed off. The `<err>` hang of 7.13e did not come back |
| 2026-11-05 | Classic | Firefox | §6 (all) | pass | Reported by Willi. §6 has now passed on both devices |
| 2026-11-06 | (not noted) | — | §8: 8.1–8.9 | pass, 2 notes | **Bug:** with Custom Chars (national letters) and group size 2, the M32 sounded one letter (ä, å) but the page expected a stray `\` with it ("Ä\", "\Å"); `\` has no Morse code. **Cause (firmware):** `getWord()` returned the last word of `player.txt` uncleaned when the file has no space or newline after it; the file ended in `\sk`, so `\` became custom character 43 (the M32's lesson menu showed `43-43: \`). Fixed on master (`getWord()` cleans that word too); the page also leaves out any character without a Morse code (2026-11-06b). See 8.11, 8.12. **Handling:** changing Koch Sequence or Koch Lesson means leaving the CW Keyer for the M32 menu and coming back. |
| 2026-11-06 | — | — | 8.10 | deferred | to the next bench |
| 2026-11-06 | Pocket | — | — | flashed | `pocketwroom` (standard) from master `ec59fe4` + the uncommitted `getWord()` fix, app only (0x10000), "Hash of data verified". For 8.12 |
| 2026-11-06 | Pocket | not noted | 8.12–8.16 | pass | Reported by Willi as "Bench passed" after the flash, page 2026-11-06c. 8.11 (the page leaving out `\` on the old firmware) was overtaken by the flash; 8.10 still deferred; the classic not flashed with the fix |
| 2026-11-06 | (not noted) | not noted | §9 (9.0–9.9) | pass | Reported by Willi as "all tests passed", page 2026-11-06e. Signs off the M4b defaults: typed and keyed answers, a missed call replayed and brought back; All characters also drives groups/words/phrases |
