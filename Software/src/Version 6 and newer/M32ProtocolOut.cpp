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

#include "M32ProtocolOut.h"
#ifdef CONFIG_BLE_SERIAL
#include "MorseBleSerial.h"     // confined to the .cpp — keeps MorseJSON.cpp free of a header cycle
#endif

M32Tee m32out;

size_t M32Tee::write(uint8_t c) {
	return write(&c, 1);
}

size_t M32Tee::write(const uint8_t *buffer, size_t size) {
#ifdef CONFIG_BLE_SERIAL
	if (m32protocol)
		Serial.write(buffer, size);
	if (bleProtocol && MorseBleSerial::linkUp())
		MorseBleSerial::txEnqueue(buffer, size);
	return size;
#else
	// without BLE the tee degenerates to a plain forward: every call site is
	// only reachable under an m32protocol gate, so behavior is byte-identical
	return Serial.write(buffer, size);
#endif
}

void M32Tee::echo(const String& s) {
	Serial.print(s);            // today's "Serial Output" behavior, handshake irrelevant
#ifdef CONFIG_BLE_SERIAL
	if (bleProtocol && MorseBleSerial::linkUp())
		MorseBleSerial::txEnqueueEcho((const uint8_t *) s.c_str(), s.length());
#endif
}
