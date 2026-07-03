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

#include "MorseBleSerial.h"

// Skeleton: the GATT server, rings and flow control land with the next
// implementation step; a module that is present but never started keeps
// every earlier build checkpoint linking with CONFIG_BLE_SERIAL enabled.

volatile bool MorseBleSerial::isRunning = false;
volatile bool MorseBleSerial::isConnected = false;

bool MorseBleSerial::linkUp() { return false; }
bool MorseBleSerial::init() { return false; }
void MorseBleSerial::stop() {}
void MorseBleSerial::pump() {}
bool MorseBleSerial::readByte(uint8_t &b) { (void) b; return false; }
bool MorseBleSerial::takeLineReset() { return false; }
bool MorseBleSerial::takeRxOverflow() { return false; }
size_t MorseBleSerial::txEnqueue(const uint8_t *buf, size_t len) { (void) buf; return len; }
size_t MorseBleSerial::txEnqueueEcho(const uint8_t *buf, size_t len) { (void) buf; return len; }
void MorseBleSerial::txFlush(uint32_t timeoutMs) { (void) timeoutMs; }

#endif /* #ifdef CONFIG_BLE_SERIAL */
