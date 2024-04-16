# Build Instructions & Change History

## Build Instructions (for Arduino IDE)

It is now quite straightforward to set up an environment to build the Morserino-32 binary code from the source. As we are using now the latest ESP32 libraries from Heltec, and the source of the Clivkbutton library has been included into the source, there is no need to hunt for libraries.

**NOTICE: As Heltec changed their development environment libraries to accomodate newer products, these instructions do not work anymore, and you might need to set up an environment with their older library versions! See item 3 below.**


1. Set up the Arduino IDE from <https://www.arduino.cc/en/Main/Software>.

2. Install on your computer the USB driver for the SiLabs CP2104 chip, used in the Heltec board for USB communication. Follow these instructions: <https://heltecdocs-fock.readthedocs.io/general/establish_serial_connection.html>.

3. <del> Install the development framework through the Arduino IDE; follow these instructions: <https://docs.heltec.org/en/node/esp32/quick_start.html>. **NOTICE: As Heltec changed their development environment libraries to accomodate newer products, this might not work flawlessly anymore!** </del>

	**Please refer to the GitHub site of Andres Lohr, DB7AL,  how to set up a development environment that works with the current source code:** <https://github.com/alohr/Morserino-32/blob/develop/Software/NOTES.md>


4. Get the source for the latest Morserino-32 firmware from GitHub (<https://github.com/oe1wkl/Morserino-32/releases>).

5. Load the .ino file into the IDE (the other source files will be loaded automatically as well), and compile (and upload).

## Change History

### Changes V.5.1.2

#### Feature Modifications:
* The timings of echo trainer were still not good enough for very huge settings of inter-word space (you might want these setting to be huge if you want more grace time after the prompt). These have been modified now and should hopefully turn out to be reasonable.
* In echo trainer mode, entering an 'error' character (7 or more dits) would just delete the last character you had entered (like a backspace). This has been changed now to a complete reset of the word. So after you realize you made a mistake, send the 7 or more dits, and then immediately start keyeing the word from the beginning.
* The nomenclature of the pitches that can be selected has been changed. Besides the values in Hz, there were the names of the nearest equivalent musical tones given, but (inconsistently) in the German nomenclature (e.g. a2 for 880Hz). This has been changed to the names common in English (A5 with this example of 880Hz).

#### Bug Fixes:
* The pitches of the side tone did not correspond to the pitches shown in the parameter menu (caused by an index error in an array), and were one step too low. Fixed.
* When connecting a straight key with a two-pole plug to the Morserino, it would always go into „Hardware Config“ at start-up (because the 2-pole plug creates a short for the 3-pole jack ring). Fixed.
* When the option „Keyer&Gen.&RX“ was selected for the parameter „Key ext TX“, the Morserino would not key a transmitter in mode „Transceiver / iCW/Ext Trx“. Fixed.


### Changes V.5.1.1

#### Feature Modifications
* CW Academy has changed the order of characters in their course. This has been taken into account in Morserino’s CWA character sequence.

#### Bug Fixes:
* When using the CW memory, the automatic playing of the memory could only be stopped when using an iambic mode, but you could not stop it when using a straight key. Fixed.
* When an IP address with 4 times 3 digits was displayed, the last digit was not completely visible on screen (because the font was too big for 15 chars on a line). Fixed (using a smaller font).
* When storing snapshots, already used snapshot locations should have been shown in bold face, but this was not the case. Fixed.


### Changes V.5.1

#### Feature Modifications / Improvements
* When selecting a WiFi setting, the peer IP name is displayed together with the SSID.
* In LoRa transceiver mode, sometimes spurious characters are received as a result of local ambient noise. Packet headers are now checked for the correct protocol version etc, which should make this a rather rare occurrence.
* Into the list of common amateur radio abbreviations, „QSZ“ („Send each word or group twice“) and „QAZ“ („Closing because of thunderstorm“) have been included.
* There is a new algorithm for calculating the  waiting time in Echo trainer; especially with very long inter-word spacing that was insanely long, more or less unusable. While it is still to some degree depending on inter-character and inter-word spacing, the differences are not so big anymore.
* The user manual now contains an appendix with all CW abbreviations used by the Morserino-32.

#### Bug Fixes:
* A pause was not played correctly when using the M32Protocol command „put cw/play/…“ or „put cw/repeat/…“. Fixed.
* When he file player reassumed playing after being stopped, it missed a word of the text (the fix in 5.0.2 fixed only part of it). Fixed (hopefully for good).
* The last characters in the CWOps Academy character sequence were in the wrong order. Fixed.


#### New Feature(s)
* Implementation of a memory keyer: 8 memory slots (with max. 47 characters per slot) can be stored permanently (but you need a computer program that implements the serial protocol in order to define these memories). But once defined, you can recall these memories also stand-alone within keyer mode (or within mode iCW/Ext Trx). Memory slots 1 and 2 will be played in a loop (until stopped by touching the paddle or key), e.g. useful for a CQ loop, the other 6 slots play just once when activated.

	For this feature there are new commands in the serial protocol:
    * PUT cw/store/<n>/<content> : store <content> in permanent memory number <n> (n is 1 .. 8); if content is an empty string, this memory is deleted. <content> can be normal Morse code characters, pro signs, e.g. <bk>, and also [p] or  \p for a pause
    * PUT cw/recall/<n>: generate morse code from the content in memory number <n>; if <n> is 1 or 2, do this until stopped by touching a paddle, or by the PUT cw/stop command
    * GET cw/memories : get a list of memory numbers that have some content stored; example:  get cw/memories -> {„CW Memories":{"cw memories in use":[1,3,4]}}
    * GET cw/memory/<n> : get contents of memory number <n>; example:  get cw/memory/1 -> {"CW Memory":{"number":1,"content":"cq cq cq de oe1wkl oe1wkl [p]"}}

	In Keyer mode (or within mode iCW/Ext Trx) you can recall a memory by pressing the black encoder knob quickly once, and then select by rotating the encoder the memory you want to be „played“
	- the number (and beginning of content) are shown in the top display line. You start the generation of content by clicking again with the encoder knob
	(if EXIT is shown and you press the encoder, you leave without anything being generated). Here also memories 1 and 2 are played continuously, until interrupted by manual entry.

### Changes V.5.0.3

#### Bug fixes:
* There was a problem introduced with the last bug fix regarding File Player, so that after uploading a new file, the file player would not start, and the M32 would not react to anything. Fixed.
* As of version 5.0, a bug had been introduced that affected WiFi and LoRa transceiver modes: if an undecodable or unrecognized character was received, a series of spurious dits and dahs were generated. Fixed.

### Changes V.5.0.2

#### Bug fixes:
* In V4.5 two more options were introduced for the feature „Word doubling“ (play each word 2x in Generator and File Player modes), to change a higher  inter-character space value when the word is repeated: "ON (true WpM)" to ignore the higher inter-character space completely , and "ON (less ICS)" to add only half of the excess inter-character space. In V.5 this change was erroneously lost, and now it has been re-introduced.
* There was a problem with the serial protocol: even when there were 8 snapshots stored, the command „GET snapshots“ would only return 7. Fixed.
* The Parameters „Each Word 2x“ and „Stop/Next/Rep“ are mutually exclusive, so if you activate one, the other should be set to OFF. This did not happen, and strange effects occurred if you set both parameters to ON. Fixed.
* When stopping file player and then restarting it, it should continue with the next word after the interruption, but it always skipped another word. Fixed.
* Under certain circumstances a DEBUG message appeared on the serial output . Fixed.



### Changes V.5.0.1

#### Bug fixes:
* A change of values for inter character space and inter word space was registered and shown on display, but only became effective after a restart. Fixed.
* In Koch trainer, the length of words and abbreviations was not handled correctly (generated things were always max l-1 long, not l). Fixed.

#### Feature Modification
* The max value for inter character space was changed from 24 to 45, and the max value for inter word space from  45 to 105, to accommodate the initial settings used by CW Academy.

### Changes V.5.0

#### Bug fixes:

* `<bk>` was not recognized as pro sign in file player. Fixed.
* `<err>` was only recognized once in echo Trainer mode, so you could only correct one error within a word. Fixed: it behaves now like a backspace key.
* Some inconsistencies in displaying snapshot functions and hardware calibration functions. Fixed.
* Paddle polarity was the opposite as shown in the preferences. Fixed.
* Touching touch paddles at power-on to get into hardware configuration menu was not very reliable. Hopefully fixed.

#### Feature Modifications / Improvements:

* `\c` or `<c>` marks the begin of a comment in a file player text file - these characters or the word containing one of these and the rest of the line are ignored.
* The order of parameters in the parameter menu has been changed slightly to make the grouping a bit more logical. The user manual has been changed to make sure the order in the manual is the same as in the M32.

#### New Feature(s):

* Implementation of a serial protocol (see separate documentation on GitHub).

 This allows (through some software on a computer connected through USB) screen or voice output of menus and settings (e.g. making the M32 usable for blind or visually impaired people), and also allows remote control of all Morserino features from the computer (like setting parameters, managing snapshots, changing speed, exiting and entering menus, uploading text files and even automated keying). The protocol is described in a separate document.
* LoRa output power is now configurable through the Hardware Config Menu, and can be set between 10 and 100 mW (10 - 20 dBm); previously it always used the default of 25 mW (14 dBm). Be aware that some countries have restriction regarding maximal power of LoRa - so check your mileage before changing the default to something higher!


### Changes V.4.5.2

#### Bug Fix(es):
* Through a stupid mistake of mine, a nasty bug was introduced into 4.5.1: a few vital bits of information were not written into non-volatile storage, and therefore not preserved through a power-off or re-boot. The information affected by the bug is the selected **Koch lesson**, and **WiFi information**. This has been fixed in this maintenance release. <b>It might mean that you need to update from 4.5.1. by USB</b>, unless you had a WiFi setup with a previous version!

### Changes V.4.5.1

#### Bug Fix(es):
* In the mode „Transceiver / iCW/Ext Trx“ the external transmitter (connected to connector 1, marked as X1_Key on PCB) was not keyed. Fixed.

#### Bug Fix / Feature Modification:
* The implementation of the „Serial Out“ parameter was buggy (and probably also confusing), and has been simplified. There are now 5 possible settings:
    * Nothing (certain errors might still be sent to serial port)
    * Keyed : essentially, what you enter through paddles
    * Decoded: what you enter through a straight key, or decoded audio
    * Keyed+Decoded: the previous 2 options combined
    * Generated: what the Morserino generates (also includes packets received through LoRa or WiFi)
    * All (= Keyed+Decoded+Generated)

#### Feature Change(s) / Improvements:
* The WiFi settings, as well as the setting for „Serial Out“, had been stored in (and retrieved from) snapshots. In most cases this just lead to confusions and unpredictable behavior. These settings are now not stored in snapshots, and never changed by recalling a snapshot. Now the following parameters are never overwritten by recalling a snapshot:
    * WiFi Settings
    * Serial Out
    * Koch lesson
    * Speed
    * Speaker volume

### Changes V.4.5

#### New Feature(s):
* Additional Koch character sequence. Thanks to help of Gavin, M0ETA, Morserino now also supports the character sequence used by the Long Island CW Club (LICW) for their new „carousel“ curriculum, including the option to set the beginning for each carousel entry point. To achieve that, set the option „Koch Sequence“ to „LICW Carousel“, and set the option „LICW Carousel“ to the appropriate starting sequence (there is an option for each LICW lesson for BC1 and BC2 to be used as entry point).
* The pro sign <BK> is now also supported.
* Additional options for word doubler (in CW Generator and File Player): there are now 3 different options for switching this on. The difference is, if the repetition is played with an increased inter-character speed (ICS). Which effectively gives you Farnsworth spacing): ON (same ICS), ON (less ICS) and ON (true WpM). ON (same ICS) works the same way as in the past, I.e. the Inter-character space that had been set is also used for the repetition. ON (true WpM) plays the repetition without additional inter-character space, i.e. play the repetition at true WpM. ON (less ICS) reduces the additional inter-character space to half its value, i.e. plays the repetition at slightly higher effective WpM. This should help the transition from Farnsworth spacing to actual spacing.

#### Bug Fix(es):
* In „Koch / Learn New Character“ blanks were disappearing when the screen scrolled up. Fixed.

### Changes V.4.4.1

#### Improvements:

* Thanks to Michael Lass the battery measurement has been significantly improved for the 2nd edition Morserinos (Board 4.x) - unfortunately this I not possible for the older ones. Re-calibration of the battery measurement is recommended!
* When using a straight key, only the tip of the 3-pole jack is being read, so you can use a 2-pole connector without getting a continuous tone (this became an issue with the last version, when we read the straight key also for starting a session in echo trainer mode etc.)

### Changes V.4.4

#### New features:

* You can set the brightness of the display, by double-clicking the RED button. There are 5 levels of brightness. Each double-click reduces the brightness a bit, when the lowest level has been reached a double click resets the display to full brightness again.
* There is a new option for the parameter „Key ext TX“ (Keyer&Gen.&RX) that allows the Morserino-32 in either LoRa or Internet transceiver mode to key a connected transmitter. This can be useful for remote control of a transmitter with the help of two Morserinos.
* There is a new parameter, „Decoded on I/O“, that can be set to „On“ or „Off“ (which is the default): normally, decoded CW that comes from an external source (when using any of the transceiver modi, or using the decoder to decode audio input) is played on the speaker (or headphones), but not sent to the external audio I/O port. With this parameter set to „ON“, the audio is also sent to the external audio I/O port.

#### Improvement:
* Using a straight key, you can now also start the sessions in Echo Trainer (and Generator) modes by pressing the straight key (so far you had to touch the capacitive paddles or use the black knob to start the session; these methods work as before, of course).

### Changes V.4.3.1

#### Bug fixes: ####

* There was a memory (heap corruption) bug in the Adaptive Random mode (introduced in V.4.3), when you had selected the very last Koch lesson („:“); after a correct response the Morserino would restart itself. This has been fixed (thanks to Oliver, DO1GDO).

### Changes V.4.3

#### New Feature (thanks to Oliver, DO1GDO):

* Adaptive Random Mode for Koch Trainer

	The "Adaptive Random" mode modifies the random selection of characters with feedback from the keyed responses. A wrong character will increase its probability to be selected. A correctly keyed character will reduce its probability.

	To start the adaptive mode start: Koch Trainer > Echo Trainer > Adapt. Rand.

	##### Remarks:

	   * Probabilities will be reset to its default every time you start "Adaptive Random" mode.
	   * The last Koch lessons / characters have a higher probability at the beginning of the session.
	   * At the beginning of the session, every character will be selected once (in random order).
	   * After every character was selected once, the next characters are selected randomly, characters that have been keyed wrong will have a higher probability to be selected.
	   * A wrong keyed character will also increase the probability of the character left and right. E.g. "z/?" is asked and you reply with "g/?". Then the probability of z will be increased and probability of / will also be increased a little.
	   * Only the first wrong character will be analyzed. Subsequent input will not be analyzed. E.g. "z/?" is asked and you reply with "gz/?". Probabilities will be increased the same way as in previous example.
	   * Do not expect to have any fun in this mode. The adaptive mode will tease you with the characters that you cannot key 100% correctly every time. Once you have keyed a character wrong, that will give you the chance to key the character wrong again and thus increasing its probability to be selected again. If you reached a total level of frustration, switch back to koch random mode and relax some time before using the "Adaptive Random" mode again.

### Changes V.4.2.1

#### Bug fixes:
* There was an annoying bug in Echo Trainer for those, who use it with a straight key: the inter-word space chosen in the parameters was not properly considered, so you had to keep really tight pauses between the characters, or the pause was considered as end of word, resulting of course in an error. Fixed (tnx to Roland, DG6RW, for doing the analysis).
### Changes V.4.2

#### Bug fixes:
* <ve> in echo mode showed an error even if entered correctly. Fixed.
* When using a transceiver mode after Koch / Learn New Character, the display would show dots and dashes for each character (as it does in Koch / Learn New Character; that mode had not been reset). Fixed.


#### Improvements:
* If Keyer Mode = Straight Key, you can now use the capacitive paddles like a cootie key / sideswiper.
* In Koch /Learn New Character: There is a blank on the display between the dots and dashes and the characters in clear text (important when the character is either a „.“ or a „-„  ); the cleartext character is also now displayed in bold.

#### New Features:
* For the Koch methods, there is now another sequence of characters that is supported: the sequence as used by CWOps CW Academy.
* In Echo Trainer mode: when you set the parameter „max # of words“ to any value but „Unlimited“ (means the echo trainer will pause after that many words), you will see on the top line of the display how many errors you made in that run (be aware that you can make repeated errors regarding one word). This will be shown for 5 seconds, then you can continue to the next run of words.


### Changes V.4.1

#### Bug Fixes:
* The Setting „Echo Prompt“ - „Display Only“ in Echo Trainer mode did not work correctly (the prompt word did not show up on the display, making this setting completely useless). Fixed.
* When using the „Stop/Next/Repeat“ feature in CW Generator, bouncing of keys could lead to erratic behavior (especially with external paddles - some of them have pretty long bounce times). So it could happen that the M32 thought you would want to stop the whole session, and the next start would begin with „vvv…“ again… Some mechanical paddles are really beasts when it comes to their bouncing behavior! Fixed (hopefully).

#### Improvements:
* Echo Trainer Mode: if you notice during your response that you made an error, you can now finish that word with an „error“ character (8 - or more - dits, will show as <err> on the display), and then do your entry from the start again. In this way you do not need to wait for the ERR message and the repetition of the prompt.

#### New Features:
2 new features have been implemented into the File Player mode:

* When playing a file, it is now possible to introduce pauses (useful e.g., when you play a QSO text - you can have longer pauses between phrases or when switching from station A to station B). Do this by using \<p> or \p (with a space before and after): each \<p> (or  [p] or \p) introduces a pause of three regular inter-word spaces. Use several pause markers (e.g. like \p \p \p ) if you want longer pauses. Be careful to have the pause marker separated with spaces from each other and from the rest of the text - if not, the whole word (e.g. cq\<p> ) will be replaced by a pause! 
You can also  use this feature in Echo Trainer mode, there will just be a longer pause, before the next prompt will be generated.

* You can also introduce tone changes in the file (useful, when you play QSO text, to distinguish station A from station B, e.g.) Do this by inserting \<t> or \t  or \[t] (as a separate word, i.e.  with at least a blank space before and after!) as a tone marker. At this point, the tone will change (unless you have set the parameter „Tone Shift“ to „No Tone Shift“), and at the next occurrence of the tone marker it will change back to the original tone. Be careful to have the tone marker separated with spaces from the rest of the text - if not, the whole word (e.g. cq\<t> ) will be considered as the tone marker, and the rest of the word (in our case „cq“) will be lost!

In Echo Trainer Mode, the tone marker is ignored.


### Changes V.4.0.1

#### Bug Fixes:
* The polarity of connections for the external paddles (and straight key) were reversed; fixed.
* In Decoder mode, the parameter „Keyer Mode“ could not be reached by double-clicking; fixed.
* Starting hardware configuration with only one touch paddle pressed did frequently not start the hardware calibration, and so it was necessary to press both paddles at start-up; fixed.

### Changes V.4.0

#### General:
* Support of new hardware (M32 2nd edition, on PCB board v 4, with Heltec WiFi LoRa module 2.1)
* Hardware Configuration is now started differently: instead of pressing the black knob while switching on (as was the method for Firmware 3.x) you press now the paddle (touch or external, or straight key) while switching on.

#### Improvements:

* Better granularity with respect to speaker output volume (especially for headphone use)
* No „weird“ tone change at very low volumes

#### Bug fixes:
* The combination of parameter „Send with LoRa“ would not send at the correct speed when using a straight key . Fixed.
* From v.3.0 on a bug had been introduced, by which the selected Koch sequence was not stored in Snapshots. Fixed (thanks Rainer, OE9RIR).

#### New Feature(s):
* Also with LoRa Trx and WiFi Trx it is now possible, like with CW Generator, to change the output of received items (or generated items in case of CW Generator) between „Char by char“ (default), „Word by word“ and „Display off“ (forcing you to decode by ear ;-) You do this by setting the parameter „CW Gen Displ“.
* The parameter „Send via LoRa“ has changed its name to „Generator Tx“, and has got three possible values now: „Tx OFF“ (= do not transmit generated CW), „LoRa Tx ON“ (transmit generated code through LoRa) and „WiFi Tx ON“ (transmit generated code through WiFi). This can be used in all CW Generator and Koch / CW Generator modes, including File Player. Could be useful for groups of learners, as you can transmit e.g. contents of a file to a group of people. Obviously this should only be used with caution (and not for extended period of time) on public M32 chat servers, but can be very handy for a group on the same network segment, using broadcast as TrX peer, or a privately set up chat server.
* It is now possible to enter and store three different WiFi configurations (thanks to Martin, EI2HIB); this can be useful when you are regularly using the M32 in different locations, or when you are using different TRX peers ( peer-to-peer, chat servers or broadcasts). There is a menu item under WiFi Functions to select one of these configurations.

### Changes V.3.0.2 (Bug Fix release)

#### Bug fixes:

* Umlaut characters (ö, ä, ü) received in LoRa or WiFi Trx were not shown correctly on the bottom line of the display (but correctly once the display scrolled up; has to do with a problem the Arduino String object has with UTF8 characters). Fixed.
* Empty WiFi Trx packets (keep alive packets as sent by chat servers) were not ignored but displayed as a space. Fixed.
* If you started WiFiTrx without configuring WiFi, and had set the „Quick Start“ option before that, the Morserino-32 would cycle through re-boots without a possibility to intercept it (you had to re-flash through USB with an erase option to completely erase all stored parameters). This has hopefully been fixed now: when you start WiFi Trx, and WiFi is not configured or not found, you will be returned to the main menu.


### Changes V3.0.1 (Bug fix release)

#### Bug fixes:
* After doing a "Check WiFi" a subsequent File Upload or Firmware Update showed IP address "0.0.0.0" and failed. Fixed.
* If you selected a maximum word length or abbreviation length of 6 for CW Generator modes, it actually responded as if the length was set to Unlimited. This bug has been around for a long time, has only been detected recently. Fixed.

#### Feature Changes:
* Wifi Trx uses now port 7373 also for outgoing packets. This should make crossing NAT firewall a bit easier in some cases.
* You can now also set a DNS host name instead of an IP address for your WiFi Trx peer. Unresolvable host names, as well as illegal IP addresses, are replaced by the broadcast address.
* When starting Wifi Trx, you will see the actual peer IP address (or "IP Broadcast") on the display for a moment.


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
