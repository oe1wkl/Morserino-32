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
| 5.1 | VoiceOver on (Cmd+F5), do 2.1–2.4 | "Listen…", "Type what you heard…", "Right." and "It was K M R. You typed K M T." are spoken without moving focus |

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
