#ifndef MORSEWINKEYER_H_
#define MORSEWINKEYER_H_

/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2026  Willi Kraml, OE1WKL                                                                            ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program.
 *  If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

/// K1EL WinKeyer (WK2, with the WK3-mode handling) emulation, so that logging and contest
/// programs can send CW through the M32. Design and every decision behind it:
/// devdocs/winkeyer/DESIGN.md (§6 decisions, §7 command-by-command verdicts, §8 logger facts).
///
/// The mode owns the serial port while it is active (D1): the M32 protocol is off, and DEBUG(),
/// the raw Serial prints that can fire at runtime and the m32out character echo are gated on
/// ownsSerial(). On the classic M32 the port runs at 1200 8N2 (9600 on request) while the mode
/// is active; on the Pocket (native USB) the baud rate is irrelevant, and a Host Open at the top
/// menu also enters the mode (D1a, auto-detect).
///
/// The M32 keeps paddle and pitch (D7): keyer mode, paddle swap, autospace, latency and the
/// sidetone pitch stay with the M32 preferences; the host's values are parsed and ignored.
/// Nothing the host sets is written to NVS, and everything reverts on Host Close or mode exit.

#include <Arduino.h>
#include "morsedefs.h"

#ifdef CONFIG_WINKEYER

#if ARDUINO_USB_CDC_ON_BOOT     // HWCDC, or TinyUSB's USBCDC in the usbio builds (VK1HF)
  #define WK_NATIVE_USB 1       // native USB: baud rate irrelevant, Host Open can be detected at the top menu
#endif

namespace MorseWinKeyer
{
  // D6: the one place the claimed firmware revision lives. 23 = WK2 v23, what the K3NG keyer
  // has reported to every major logger for years. Try 31 (WK3.1) on the bench, then settle.
  const uint8_t REVISION = 23;

  extern volatile bool portOwned;
  inline bool ownsSerial() { return portOwned; }   // gates DEBUG(), m32out.echo(), runtime Serial prints

  // menuExec(): the mode takes the port. `remote`: Remote WinKeyer (DESIGN.md §9.1) - the keying goes to a remote
  // rig over M32KIP (MorseKipKeyer); host CW plays only while that link is up, and is dropped while it is down (R4).
  void enter(bool autoOpened, bool remote = false);
  void leave();                         // menu_(): gives it back; no-op unless the mode was active
  void serviceInput();                  // serialEvent() while the port is owned
  bool takeTickServiced();              // serialEvent(): tick() already polled the port in this loop() pass
  void tick();                          // loop(): sender, timers, output queue
  void paddleActivity(bool pressed, KEYERSTATES ks);  // loop(), before doPaddleIambic(): break-in
  void paddleChar(const String& symbol);              // displayDecodedMorse(): paddle echo
  void encoderSpeed(int t);             // loop(): the encoder in its speed role (E1)
  void stopSending();                   // loop(): black-knob click - abort host CW and tune
  void beforePreferences();             // around the preferences menu: no host speed may reach NVS
  void afterPreferences();
  bool wantsMenu();                     // an auto-entered session was closed by the host
  bool isOpen();
  bool hostSending();                   // the sender is busy (a letter, a timed mark, tune): no redraws now
  bool sidetoneWanted();                // keyOut(): pin config sidetone bit, paddle-only sidetone
  bool txWanted();                      // keyOut(): key-output bits, paddle watchdog, paddle mute
  // loop(), classic only (FN on GPIO0): a logger that sets DTR on / RTS off pulls GPIO0 low through
  // the CP2102 auto-reset circuit, i.e. holds FN down for as long as it has the port open. Takes
  // the FN button's clicks and debounced state, returns the clicks the mode should act on.
  int filterFn(int clicks, bool down);

#ifdef WK_NATIVE_USB
  // D1a: armed only around the top menu's serialEvent() call. topMenuByte() takes the WinKeyer
  // bytes (0x00 admin, 0x13 null, and the parameters of a command in progress) before the M32
  // protocol's line assembly sees them, and returns true for every byte it took.
  extern bool topMenuArmed;
  bool topMenuByte(uint8_t c);
  bool takeAutoOpen();                  // a Host Open arrived at the top menu (already answered)
#endif
}

#endif // CONFIG_WINKEYER
#endif /* MORSEWINKEYER_H_ */
