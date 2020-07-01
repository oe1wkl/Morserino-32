# Build Instructions & Change History

## Build Instructions (for Arduino IDE)

It is now quite straightforward to set up an environment to build the Morserino-32 binary code from the source. As we are using now the latest ESP32 libraries from Heltec, and the source of the Clivkbutton library has been included into the source, there is no need to hint for libraries.

1. Set up the Arduino IDE from <https://www.arduino.cc/en/Main/Software>.

2. Install on your computer the USB driver for the SiLabs CP2104 chip, used in the Heltec board for USB communication. Follow these instructions: <https://heltec-automation-docs.readthedocs.io/en/latest/general/establish_serial_connection.html>.

3. Install the development framework through the Arduino IDE; follow these instructions: <https://heltec-automation-docs.readthedocs.io/en/latest/esp32+arduino/quick_start.html>.

4. Get the source for the Morserino-32 firmware Version 3.x from GitHub (<https://github.com/oe1wkl/Morserino-32/tree/master/Software/src>).

5. Load the .ino file into the IDE (the other source files will be loaded automatically as well), and compile (and upload).

## Change History

### Changes V3.0

#### General:
* Added a section in the user manual, how to update the software through USB (in case you have trouble using WiFi for the update).
* Cleaned up the code and started to modularize it. First attempts to actually use C++ code and not just C (;-) Use the newest Arduino IDE and the up-to-date libraries from Heltec for the ESP32 (to me it seems, WiFi got a bit more reliable now…). Clickbutton library now included in source code, so no need to find and install it if you want to compile from the source.

#### Bug fix:
* File Player in Echo Trainer mode would always stop at end of file; you had to stop the mode (e.g. with pressing the black knob) and then restart, to continue from the beginning. This has been fixed.
* There was a potential for a buffer overflow using LoRa (pointed out by Timo Tomasini). That has been fixed.

#### Feature Change:
* CW Generator / AutoStop option: This option is available for CW Generator and File Player modi, and is now called „Stop/Next/Rep“ (Stop - Next - Repeat) and works slightly differently than before: it generates one word (also visible on the display), then stops and waits for paddle input. A press of the left paddle will repeat the current word, while a press on the right paddle will generate the next word. This is useful for training your head copy proficiency: let it play a word (without looking at the screen), and try to decode it in your head, if you are not sure, press left for repeat; if you think you got it right, compare it with the display. Now you can either repeat it again (left press), or look away and press the right paddle for the next word. (You can remember the functions of left and right paddle by thinking of typical player buttons - left is back, right is forward.) Please note that the options Word Doubler and Stop - Next - Repeat are incompatible with each other - if you set one to ON, the other will be set to OFF automatically.
* The word list of common English words (used for CW Generator and Echo Trainer, also in Koch mode) has been extended from 204 words to 373 words.]
* In File Player: pro signs can now also be noted with a prepended back slash as \sk, \kn, \ar etc.  (in upper or lower case); this is the notation used by „Just Learn Morse Code“.

#### New Feature(s):
* Support for Straight Key. All functionality, especially Echo Trainer and Koch Echo Trainer, but also the Transceiver modes, now support the use of a straight key. In order to use a straight key, you have to set the Keyer Mode parameter to Straight Key. If this setting is used, the external key may also us a 2-pole jack. (For the „Stop - Next - Repeat“ feature - see above - you will need a paddle; use the built in one, if you don’t have an external paddle.
* In addition to communicate through LoRa between Morserinos, it is now also possible to communicate through WiFi (using UDP), thanks to Dominik Heidler. You can communicate easily on the same local network, but with either VPN or port forwarding on your Internet router you can communicate across the globe… And you don’t need to hook up a computer for that. Another unique feature no other Morse code trainer can offer! You have to set the IP address of your communication partner through WiFi Config, and then select Wifi Trx in the Transceiver menu. If you are inside a firewall, you need to tell your partner your outside IP address, and set up port forwarding for port 7373 to your Morserino inside. Or alternatively you could connect your networks through a VPN.
* CW Generator and Echo Trainer with custom character set. If you want to train a specific set of characters (your weak points…), you can do this now. You upload a text file for the file player that contains the characters you want to train (as one „word“ or several, in one line or more), and then set the parameter „Koch Sequence“ to the new option „Custom Chars“. This reads the characters from the file. Now you can use the Koch Trainer (CW Generator or Echo Trainer), and it will use exactly those characters for your training (the setting of the Koch lesson has no influence at this point). If you want to change the character set, upload a new text file, and re-select the option „Custom Chars“ (even if it had been set before), to prepare the new character set (if you just upload a new text file, the custom character set will not change - you have to go into parameters and re-select „Custom Chars“ again; this is a feature, not a bug: it means you can switch between training your characters, and using a (different) text file for file player…). Setting „Koch Sequence“ to M32 or LCWO will revert to the „normal“ Koch trainer option.

### Changes V2.4

#### Bug fix:
* There was a slight timing error regarding the space between elements of a morse character in all Keyer functions (Keyer, Echo Trainer), making this interval around 5 ms too long (regardless of the speed setting). It was caused by checking the paddles (unnecessarily) several times, and each paddle check added 1-2 ms of delay. This has been fixed.

#### Feature expansion:
* The option to output characters via USB (to a PC, for example) has been extended. Apart from the  options “Keyer“ (default), „Decoder“, „Keyer+Decoder“, or „ERRORS only“ there is now another option „Everything“, that outputs practically all characters in all Modes (including CW Generator, Echo Trainer) to the serial bus; almost everything you see on the display, with the exception of the status line, or any menus.

### Changes V2.3

#### Bug fix:
* After using any of the WiFi functions, battery measurement does not work correctly until the Morserino-32 is powered down and up again (or a reset with the Reset button has been performed). This is due to a hardware problem on the Heltec board. The voltage shown was incorrect. This cannot be fixed, but: In such cases the Morserino-32 displays now “Unknown" instead of the battery voltage, and the battery symbol is shown with an inscribed question mark. After a power cycle (or a rest with the reset button) everything should work OK again.


### Changes V2.2

#### Bug fixes:
* Recalling a snapshot caused a reset of the file pointer for playing a file, so after the recall it would always start at the beginning. Fixed.
* In CW Generator restart using the paddle did not work correctly (the vvv<ka> sequence was missing, and the first element of the next character got lost). Fixed.
* Max Words was incorrectly stored in snapshots; it was impossible to store the value „UNLIMITED“. Fixed.
* When exiting a modus in scroll mode, and having scrolled up one or more lines, the display did not work correctly entering the same or another modus (it still was scrolled and displayed things on a line not shown - entering scroll mode and scrolling down would show everything). Fixed.

#### New features:
* Re-introduced line break after each word in Echo Trainer mode and Koch Learn New Character mode
* Option to output keyed and/or decoded characters on USB. For this there is a new parameter called „Serial Output“, which can be set to Keyer (default), Decoder, Keyer+Decoder, or ERRORS only (this outputs some debugging and error messages to USB, but only when none of the other options has been selected). The serial output works in the following modi: CW Keyer, CW Decoder and Transceiver/iCW/Ext Trx. You could use a simple terminal program to display these characters (115200 baud).
* Possibility to calibrate battery voltage measurement. As the built-in capability of Heltec modules to measure LiPo battery voltage sometimes shows large errors, apparently depending on various factors and batch of production run, there is now an option to calibrate this measurement to get better indications of battery status. See updated manual for more details.
* 2 new abbreviations have been added to the CW and Echo Trainer: mm (maritime mobile) and loc (locator / located).


### Changes V2.1

#### Bug fixes:
* In Generator Mode only vvv<ka> was sent when the option Auto Stop was on. This has been fixed. In addition, the parameter „Max # of Words“ is ignored when „Auto Stop“ is „on“.
* In ‚Echo Trainer' > ‚Random' (length = 1 char) everything was running OK until a '+' occurred. This sign was then repeated in an endless loop, no matter if the answer was correct or not. This has been fixed.
* In The 'echo trainer' with „Max # of Words“ not unlimited, when the last word was repeatedly given wrong, the Morserino hung in a strange state (which could be exited with a press of the black knob). This has been fixed.
* The CW decoder was setting the keyed speed according to the code it was decoding (described in the manual as „bug or feature“). This was a nuisance in Transceiver mode, when you wanted your keyer speed to remain the same no matter what speed the other station was sending with. This has been fixed, decoder speed is now independent from keyer speed. In Transceiver mode both speeds are shown as e.g. r14s18 WpM, which means received 14 WpM, sending at 18 WpM.

### Changes V2.0

#### Bug fixes:
* Time-out Option: the selection „No timeout“ did not get saved, so after restart the selection „5 minutes“ was active again. This has ben fixed now.
* Effective wpm was not correctly calculated (pointed out by kf9up). This has been fixed.
* Cleaned up a few cases where the display was not quite correct


#### New features:
* Auto Stop : Stops the generating of morse characters in CW Generator and Koch Generator modes after one word (or letter group) to help with learning head copying. One word is being played (without being shown); continue by touching the paddle or clicking the black knob, and the word will be shown. Touch again to play the next word. This is enabled by setting the parameter „Auto Stop“ to On.
* Pausing CW generator or Echo Trainer automatically after a certain number of words or groups. There is a new parameter „Max # of words“ , that you can set to Unlimited (= default value) or to a value between 5 and 250 in steps of 5. When the specified number of words or letter groups has been generated, the Morserino-32 will pause automatically (as if you had clicked the black knob) and wait - with a touch of a paddle (or clicking the black knob) it will continue and generate the next n words.
* Snapshots: it will be possible to store up to eight snapshots of the current parameter settings in non-volatile memory. These can be quickly recalled (ans also deleted, if not need anymore). So if your are training with different settings and have to switch these parameters now all the time, you will save a lot of time by using this feature. Ig stores the current value of all parameters (but not the Koch lesson you had selected - that stays the same even when you recall a snap shot).
    * You store a snapshot by a long press of the RED button while in the parameters menu, and you recall a snapshot by a short press of the RED button while in the parameters menu. You can select a snapshot number between 1 and 8 for storage, and for recall only those numbers that have been stored.
* Two virtual „channels“ for LoRa (Standard and Secondary). Both operate on the same frequency, but use the LoRa „Syncword“ to create effectively two independent channels. Useful if you want to set up two independent groups within an area, eg. for training classes. Select the secondary channel by changing the parameter „LoRa Channel“.
* Support for LoRa @ 860 - 925 MHz. If such  a Heltec module for the upper UHF LoRa frequencies is being used, you need to configure the Morserino by pressing the BLACK knob at startup, then you can choose 1. a band, and b. a frequency (QRG) within the band (starting with a default QRG for that band). You can also set a different frequency for the default 433 MHz module if you desire.
* Check Wifi (see below)


#### Other Changes:
* All WiFi functions are now available as a separate entry („WiFi Functions“) in the main menu (instead of the triple click), with the following functions underneath
    * Disp MAC Address - show MAC address of the M32 (same function as before, just relocated into main menu)
    * Config WiFi - same function as before, just relocated into main menu)
    * Check WiFi - new function: checks connectivity with the credentials you supplied; if positive, shows also the IP address
    * Upload File - same function as before, just relocated to main menu
    * Update Firmw - same function as before, just relocated to main menu
* Minor changes in the order of characters for the Koch method (to bring them better in line with Just Learn Morse Code)
* Undecodable / unrecognized characters are now displayed as * on the display
* When setting parameters (or storing / recalling snapshots) while CW Generator or Echo Trainer is active, you will have to re-start the activity when returning from the parameter menu, by touching a paddle.
* The User Manual has been overhauled, and the typography of the pdf version is a lot better now.
