# Morserino-32 Pocket — Frequently Asked Questions (FAQ)

This FAQ is compiled from the official user manual and from community discussions on groups.io. It covers the **M32 Pocket**; where an answer applies to the classic Morserino-32 (1st and 2nd edition) as well, it says so. It is a companion to the user manual, not a replacement — the manual describes every mode and preference in full.

There is one manual per Morserino, and these links always point at the one for the latest released firmware:

| Your Morserino | English | Deutsch |
|---|---|---|
| **Morserino-32 Pocket** | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_EN.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_EN.epub) | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_DE.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_DE.epub) |
| **Morserino-32, 1st / 2nd edition** | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_EN.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_EN.epub) | [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_DE.pdf) · [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Classic_DE.epub) |
| **Pocket, Accessibility Edition** | [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_EN.epub) · [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_EN.pdf) | [EPUB](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_DE.epub) · [PDF](https://github.com/oe1wkl/Morserino-32/releases/latest/download/Morserino-32_User_Manual_Pocket_Accessible_DE.pdf) |

Answers reflect **firmware version 9.0**.

## 1. Coming from a 1st or 2nd Edition Morserino

* **I have an older (1st or 2nd edition) Morserino. Should I get an M32 Pocket?**
    * There is no need to replace a working one. Both models run the **same firmware**, from the same release, and get the same updates. Every training mode is on both: the Koch trainer, CW Generator, Echo Trainer, the transceiver modes, the QSO Bot, the Bluetooth keyboard and BLE Serial, the WiFi/ESP-NOW transceiver, and the serial protocol with the Configuration Tool.
    * **What the M32 Pocket gives you that the older models cannot:**
        * It is smaller, and it has a closed case — so it really does go in a pocket.
        * A colour display showing four or five lines of text instead of three on a monochrome OLED, with selectable colour themes and a **Font Size** setting.
        * Noticeably better audio: a dedicated audio codec instead of a PWM-generated tone, so the sidetone is a clean sine, its edge shaping is adjustable (**Tone Softness**), and you can upload your own Echo Trainer sounds.
        * The seven **games**, and **practice statistics** for the Koch trainer.
        * The **Accessibility Edition**, which reads the interface aloud — available only for the Pocket.
        * USB-C instead of Micro USB.
    * **What you would give up:**
        * **LoRa.** The 1st and 2nd editions have a LoRa transceiver with an SMA antenna connector (around 433 MHz); the M32 Pocket does not. Since both the older Morserinos and the Pocket offer comparable functionality over ESP-NOW, this will rarely sway your decision.
        * The **trimmer potentiometers** — the audio input level trimmer, and on the 2nd edition the headphone level trimmer. The Pocket has neither.
    * **An honest summary**: if your Morserino works, and you do not particularly want the games, the statistics, the spoken menus or the better sound, there is no compelling reason to buy a second device. You are not missing out on the CW training itself, which is what both machines are for. Buy the Pocket for its size, its display and its audio, or because you need the Accessibility Edition. Keep the classic if you depend on LoRa — and note that a good many owners simply keep both.

* **I have (or had) an older Morserino and now have an M32 Pocket. What are the important gotchas?**
    * **The audio jack is wired differently, and getting this wrong can destroy the device.** On the 1st and 2nd editions the **sleeve is audio out**. On the M32 Pocket the jack is 4-pole (TRRS) and the **sleeve is audio in**. Do not reuse a cable or adapter that worked on your old Morserino — it would feed your receiver's audio straight into the Pocket's output. Section 5 describes the splitter cable you actually need.
    * **A different battery.** The 1st and 2nd editions use a flat 3.7 V LiPo cell. The M32 Pocket takes a **14500 Li-Ion cell** — AA form factor, 3.6–3.7 V, at most 52 mm long and 14.5 mm across, and a protected cell is recommended. They are not interchangeable in either direction.
    * **A different USB connector.** Micro USB on the older models, **USB-C** on the Pocket. The Pocket also draws more while charging: up to about 500–600 mA, against about 200 mA.
    * **No LoRa.** The LoRa transceiver modes are simply not there; use the WiFi transceiver instead.
    * **No trimmers.** There is no audio input level potentiometer to turn — you set the level at the source instead, as described in section 5.
    * **The FN button is a cut-out in the case**, to the lower right of the encoder, rather than a protruding red button. More than one owner has spent a while looking for it.
    * **One gesture means something different.** A long press of FN while in a menu starts the audio input level adjustment on the older models; on the Pocket it merely keys the transmitter and produces a sidetone.
    * **Your settings do not come across.** There is no backup-and-restore between devices, so a new Pocket starts on factory settings: expect to set your call sign, Koch lesson, speed and preferences again. Snapshots do not transfer either.

## 2. Power Supply, Battery, and Charging

* **What type of battery should I use?**
    * Use a **14500 Li-Ion (3.7 V) cell**. 14500 cells have the same form factor as common AA batteries, but we need 3.6–3.7 V cells, i.e. Li-Ion. The maximum size is a length of 52 mm and a width of 14.5 mm.
    * **Recommendation**: Use a **protected cell** (one with built-in protection against deep discharge) to prevent early battery death. Community members frequently recommend high-quality brands like Keeppower (800 mAh) or XTar.
* **Where can I buy a 14500 battery?**
    * Not something the manual covers. Community discussions point to general retailers (Amazon, brands such as JESSPOW) or specialised battery shops.
* **Do I need a specific USB-C charger?**
    * No. Any normal **5 V USB charger** (like a standard phone charger) will work.
    * The M32 Pocket consumes a maximum of **500–600 mA** when charging the battery.
* **How do I charge the battery?**
    * Connect the USB-C cable and ensure the **Power Switch is in the ON (I) position**. The battery will **not** charge if the hardware switch is OFF.
    * **Status indication**: The M32 Pocket displays a battery/charging icon on the top line of the display whenever you are in a menu.
* **Low battery "gotcha"**: If the battery voltage is dangerously low, an empty battery symbol will appear and the device will not boot. Symptoms like loss of audio during keying suggest that the battery voltage is too low and the battery needs recharging.

## 3. Cases and 3D Printing

* **Where can I find 3D printing files?**
    * While the manual doesn't list URLs, community members point to **Printables** (e.g. Model 1550518 by Michael K Johnson/KZ4LY) for FreeCAD, STEP, and STL files for the M32 Pocket.
* **How do I move the sensor pads to a new case?**
    * The touch paddles are **capacitive** and made from single-sided PCB material.
    * **Process**: The PCB material is glued to the case — a bit of acetone can be used to weaken the glue so that you can carefully remove the PCBs from the original case and attach them to the new one. Be careful to reconnect the wires in the same way as they were with the old case.
* **The "missing" red button**: On the M32 Pocket, the "red button" (FN button) is **integrated into the case** rather than being a separate protruding part. It is a cut-out in the case, located to the lower right of the rotary encoder.

## 4. External Keys and Connections

* **Which jack do I use for external keys?**
    * Use the **3.5 mm 3-pole (TRS) jack** labelled "External Paddle". It is the jack closest to the USB connector.
* **Wiring guide**:
    * **Dual-lever paddle**: Tip = Left, Ring = Right, Sleeve = Ground.
    * **Straight key**: Tip = Key, Sleeve = Ground.
* **Configuration**: To use a straight key or a bug in Echo Trainer or Transceiver modes, you must change the **Keyer Mode** preference to **Straight Key**.
* **Can I connect a mechanical key to the internal paddle connector?**
    * Yes, though this is still **experimental**. The connector on the PCB that normally serves the Pocket's capacitive touch paddles (CN3) can be switched to accept a mechanical key or paddle instead, via the **Hardware Config** menu. This is useful if you want to build a mechanical key into a case together with the M32 Pocket.

## 5. Audio Input and Output

* **What is the pinout for the Audio I/O jack?**
    * The M32 Pocket uses a **3.5 mm 4-pole (TRRS) jack**.
    * **M32 Pocket pinout**: Tip and 1st Ring = Audio/Headphones Out; 2nd Ring = Ground; **Sleeve = Audio In**.
    * **Note**: This is different from the 1st/2nd edition Morserinos, where the sleeve was audio out.

* **Can I use a simple audio cable from my receiver/transceiver to the M32 Pocket?**
    * **No — do not use a plain two- or three-pole audio cable to connect a transceiver to your M32 Pocket.** The audio voltage from your receiver would be fed into the audio *output* of the M32 Pocket and would probably destroy it.

* **How do I get audio from a transceiver into the M32 Pocket for CW decoding?**
    * You need a **splitter cable: 1× 3.5 mm CTIA 4-pole TRRS plug to 2× 3.5 mm sockets**.
    * Plug the 4-pole plug into the M32 Pocket's headset socket, and your headphones into the green (headphone symbol) socket of the splitter.
    * Connect the transceiver's headphone or external-speaker output to the pink (microphone symbol) socket of the splitter, using a 3.5 mm cable. The transceiver's own speaker will go quiet at this point, as expected.
    * Tune the receiver to a good, clean CW signal and **turn its volume right down** — it is essential not to overdrive the M32 Pocket's input.
    * Put the M32 Pocket into decoder mode and set its volume to roughly one third. You will hear nothing in the headphones at first.
    * Now raise the transceiver's volume slowly. Once the M32 Pocket detects a decodable signal it starts working: you hear a beep for every dit and dah detected, and a small black square blinks in the upper left corner of the display.
    * Take care not to overload the input — if you do, decoding stops and no sound reaches your headset.
    * While no decoding is happening the headphone output stays silent, so you cannot listen while tuning around. Unplug the cable to tune, then plug it back in.

* **How do I adjust audio levels?**
    * Use the **FN button** to toggle between speed and volume control while a mode is active.
    * On the M32 Pocket, a **long press of the FN button while in a menu** keys the transmitter (if one is connected) and produces a sidetone — useful for setting levels on a connected computer or transceiver. A click of the FN button turns it off again. (On the classic Morserino-32 the same gesture instead starts the audio input level adjustment.)

* **My own success/error sounds became quieter with version 9. Why?**
    * Because they are no longer being distorted. Until version 9 the audio chip's internal gain was set so high that everything was clipped inside the chip before it ever reached the volume control. Version 9 fixes that, so uploaded sounds are reproduced cleanly — and cleanly means quieter than the distorted version was.
    * If you want them louder, prepare the files a little louder yourself, but keep their peak level **at or just below −1 dBFS**. A file normalised to the full 0 dBFS will still distort, because the MP3 decoder runs out of headroom before the sound chip does.
    * You can replace the two Echo Trainer confirmation sounds by uploading `/sounds/success.mp3` and `/sounds/error.mp3` with the Configuration Tool. Without such files, the built-in beeps are used.

* **The CW sidetone sounds different since version 9.**
    * Also the same fix. The sidetone is a clean sine wave now instead of the harmonically rich sound it used to be. On the small built-in loudspeaker that can seem slightly less penetrating, even though it is not actually quieter.

## 6. Bluetooth and VBand

* **How do I enable Bluetooth?**
    * Use the preference **Bluetooth Use**. Besides **No Bluetooth**, the options are four keyboard modes (VBand Kbd, Decoded output, VBand+Decoded, Generic Kbd) and **BLE Serial** (the M32 Serial Protocol over Bluetooth, see below).
    * Because **Bluetooth Use** is a single selector, Bluetooth serves either the keyboard output or the serial protocol — never both at once. Any WiFi function (multiplayer games, upload, update, WiFi transceiver) suspends the Bluetooth connection until you return to the main menu.
* **How do I connect to VBand?**
    * The Morserino can connect to **VBand** (hamradio.solutions/vband) via Bluetooth.
    * Community tips suggest making sure your browser supports the necessary protocols, and setting the VBand input to "Straight Key" if your paddles aren't recognised correctly.
* **Bluetooth keyboard**: The M32 Pocket can act as a **Bluetooth keyboard**. Decoded characters are sent as if typed on a US QWERTY keyboard.

* **Can apps connect to the Morserino over Bluetooth (e.g. from an iPhone)?**
    * Yes. Set the preference **Bluetooth Use** to **BLE Serial** (it takes effect on the next return to the main menu). The device then offers the full **M32 Serial Protocol** over Bluetooth Low Energy, visible as "Morserino-32", so apps can remote-control it and send text to be keyed as CW — the same protocol that is available over USB. See Appendix 8 of the user manual.
* **I set BLE Serial, but the app cannot connect. What am I missing?**
    * Since version 9 the Morserino **asks your permission on the device** before it hands over control. When an app requests a session the display shows **"Allow connect?"** and waits for you to press **FN**. The reasoning: a USB cable has to be plugged in by someone standing next to your device, while anything within radio range can connect over Bluetooth.
    * You are only asked **in the main menu**. If a request arrives while you are in a training mode it is refused, without interrupting you — so leave the device in the main menu when you expect an app to connect.
    * An app that reconnects **within a minute** is let straight back in without asking again.
    * While a session is open, a small Bluetooth symbol in the top line shows that something is connected.
    * No pairing in the Bluetooth-settings sense is needed; the consent prompt on the device replaces it.

* **Can I use Bluetooth to stream audio?**
    * No, and it would not be useful either: Bluetooth audio introduces a noticeable delay, which makes correct keying very hard.

## 7. The Accessibility Edition (M32 Pocket only)

* **What is it?**
    * A separate edition of the firmware that **speaks the menus and settings aloud**, for blind and partially sighted operators.
    * Three things are left out of it, and nothing else: the **games**, and the two WiFi entries **Upload File** and **Update Firmw**. The games are unusable without sight. The other two do not do the job themselves — they only put the Morserino into a waiting state and expect you to finish the work in a browser on another computer, which is an awkward thing to be sent to when the device cannot tell you what is happening. Both jobs are done better from the computer in the first place: use the installer page for firmware, and the Configuration Tool for uploading a file. Everything else — every mode, every preference — is present.
* **How do I get it onto my device?**
    * A new Pocket is delivered with the Standard edition, which is silent. Install the Accessibility Edition from the same installer page as any other firmware (`morserino.info/install.html`) — it is offered as a choice for the same device.
    * You can move between the Standard and the Accessibility Edition at any time. Your preferences, snapshots and stored speed survive the change in both directions. **Only the File Player's text does not**, since that is the storage the voice clips take over.
* **It used to speak and now it doesn't — is it broken?**
    * Almost certainly not. The voice clips are stored separately from the program, so the two can come apart: re-installing the program on its own, or an installation interrupted part-way, can leave a Morserino that runs perfectly — keyer, generator, decoder and all — with nothing left to speak with, or with the clips of an older version.
    * The device tells you when this has happened. On switching on you hear **four pairs of alternating high and low tones** (a two-tone alarm of about two seconds), and the start-up screen says which case it is:
        * **"No voice clips!"** — there are no clips at all; the device stays silent apart from the alarm.
        * **"Wrong voice pack!"** — the clips belong to a different version. Most still fit, so it goes on speaking; what it cannot say are the entries added or reworded since those clips were made, and those stay silent.
    * The remedy in both cases is to run the installer again, choosing the **Accessibility Edition** and **Keep my settings**. That installs the program and its matching voice clips together.
    * The alarm is deliberately neither speech nor Morse code: it has to work when speech is exactly what is missing, and it must not assume you can already read Morse.

## 8. Firmware Updates

* **What is the easiest update method?**
    * **Webserial**: visit `morserino.info/install.html` in a supported browser. It requires no command-line tools and no separate firmware download.
    * On the **Accessibility Edition** this is the only way: its WiFi menu has no *Update Firmw* entry, because that entry only hands the job over to a browser on another computer anyway. Run the installer there instead.
    * There is now a **single installer for every Morserino**. It asks the processor in your device which Morserino it is, so you no longer need to know whether to start on a page for the classic M32 or one for the M32 Pocket. It also shows which firmware version is currently on the device before you install anything, and lets you choose whether your settings are kept or erased. The two former installer pages forward to it, so old bookmarks keep working.
* **Which browsers work?**
    * **Google Chrome**, **Microsoft Edge**, **Opera**, and **Firefox from version 151** (which added Web Serial support in May 2026). **Safari cannot do this.**
    * A desktop or laptop computer is required — Web Serial is not available on phones or tablets.
    * If you use Firefox in a managed/corporate installation, note that **Firefox Enterprise Policies can disable Web Serial**, in which case the installer will not see your device even on a recent version.
* **Major "gotchas"**:
    * **Stay awake**: The M32 Pocket **must be turned ON** and **must not be in sleep mode** when you connect for an update.
    * **Cable quality**: Use a **data-capable USB cable**, not a "charging-only" one.
    * **A dark screen right after an erase is normal**: after an install with the erase option — or on a brand-new device — the Pocket can take up to about ten seconds to show anything while it prepares its file system. Since version 9 it displays an informative splash screen while this happens, so you can tell it apart from a failure.

## 9. Training Features and Settings

* **What are "Practice Sets"?**
    * They replace the earlier custom character sets. You pick a subset of individual characters directly on the device, then train just those in the CW Generator or the Echo Trainer.
* **And "Custom Characters"?**
    * A custom character order now behaves exactly like one of the built-in Koch sequences: Select Lesson, Learn New Character and the other Koch methods all work with it in the same way.
* **Can I see how well I am doing?**
    * Yes. When you practise with the Koch trainer, the M32 Pocket records **practice statistics**. You can look at them through the Configuration Tool, or through the Morserino's own built-in web server with a browser.
* **The text in the scrolling area is too small / too large.**
    * The **Font Size** preference switches the scrolling display area between the normal text size and a smaller one. The smaller size fits more characters per line and shows five lines instead of four — handy at higher speeds, or with longer words when you want to see more of what has just been sent.
* **The CW tone sounds clicky, or too soft.**
    * **Tone Softness** sets the time constant of the tone shaping that removes key clicks. The default is 5 ms; you can set anything from 1 ms (rather hard, with clicks) to 9 ms (definitely too long for high-speed CW).
* **What does the QSO Bot do?**
    * It simulates a QSO partner, for Contest, SOTA/POTA and standard QSOs. A **difficulty** preference varies how demanding the exchange is, and your partner may work at a different speed from you.
* **My snapshots behaved oddly on an older firmware.**
    * There was a bug that could fill the device's settings storage and stop snapshots being saved correctly. It is fixed, and snapshots are now stored much more economically. At the first boot after updating, existing snapshots are converted once to the new format — this can take a few seconds, and happens only that one time.

## 10. Games (M32 Pocket only)

* **Which games are there?**
    * Seven: **Morse Invaders**, **Fight the Pileup**, **Radio Cave**, **Morsel**, **Trailblazer**, **Fox Hunt** and **Memory Chain**. They are described in the user manual.
    * Most of them respect your current Koch lesson settings, so they train the characters you are actually working on (the exceptions are Fight the Pileup, which works with call signs, and Radio Cave).
    * Four of them — Fight the Pileup, Morsel, Trailblazer and Fox Hunt — can also be played against a second Morserino, or against several.
* **Why does my device have no games?**
    * Either it is a classic Morserino-32 (the games are M32 Pocket only), or it is running the **Accessibility Edition**, which does not include them.
* **How do I clear the high scores?**
    * **Reset Scores** in the preferences menu on the device. The Configuration Tool can do the same, and also shows the stored score tables — including whether Radio Cave has a saved game — on its User Identity tab.
