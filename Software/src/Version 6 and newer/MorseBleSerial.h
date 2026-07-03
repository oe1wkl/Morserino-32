#ifndef MORSEBLESERIAL_H_
#define MORSEBLESERIAL_H_

/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2025  Willi Kraml, OE1WKL                                                                            ***
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

#ifdef CONFIG_BLE_SERIAL

#include <Arduino.h>

/// "BLE Serial": the M32 serial protocol over a Nordic-UART-Service GATT
/// server, so apps (e.g. iOS/CoreBluetooth) can speak the line-based protocol
/// without USB. One central at a time, no bonding — same trust model as
/// plugging in a USB cable.
///
/// Threading contract: all GATT callbacks run on the Bluedroid host task and
/// only set flags/marks or produce into the RX ring (single-writer indices);
/// everything else — pump(), readByte(), enqueue, init/stop — runs on the
/// Arduino loop task. After stop(), every API here is a defined no-op and
/// readByte() returns false, even when stop() was reached from inside the
/// bleSerialEvent() dispatch chain (remote self-disable via PUT config).

namespace MorseBleSerial
{
	extern volatile bool isRunning;                        // GATT server up; set false FIRST in stop()

	bool linkUp();                                         // isRunning && connected && no session reset pending
	bool blocksBtKeyboard();                               // mutual exclusion: pref on AND server running (PLAN D3)
	bool init();                                           // defensive: false + visible error if the BLE stack is unusable; never blocks
	void stop();                                           // synchronous teardown incl. bleProtocol — never trusts onDisconnect to fire
	void stopWithNotice(const String &msg, uint32_t flushMs);  // protocol message (best-effort) + flush + stop
	void suspendForWifi();                                 // the one wording/timeout for every WiFi bring-up path
	void pump();                                           // loop task: session reset, advertising restart, flow-gated TX drain
	bool readByte(uint8_t &b);                             // consume one RX byte (loop task); false after stop()
	bool takeLineReset();                                  // read-and-clear: caller must discard its partial input line
	bool takeRxOverflow();                                 // read-and-clear: RX ring overflowed, current line is torn
	size_t txEnqueue(const uint8_t *buf, size_t len);      // protocol replies: bounded self-drain, then drop
	size_t txEnqueueEcho(const uint8_t *buf, size_t len);  // echo channel: drop-immediately (keying path — must never stall)
	void txFlush(uint32_t timeoutMs);                      // pump until TX empty or timeout; no-op after stop()
};

#endif /* #ifdef CONFIG_BLE_SERIAL */
#endif /* #ifndef MORSEBLESERIAL_H_ */
