#ifndef MORSEBLUETOOTH_H_
#define MORSEBLUETOOTH_H_

/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2025  Willi Kraml, OE1WKL; Brian N6UGP                                                               ***
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

#ifdef CONFIG_BLUETOOTH_KEYBOARD

namespace MorseBluetooth
{
	// Forward declarations
	inline boolean isBLErunning = false;
	void initializeBluetooth(void);
	void stopBluetooth(void);
	void bluetoothTypeLCTRL(bool ctrl);
	void bluetoothTypeCharacter(const char chr);
	void bluetoothTypeString(const String str);
};

#endif /* #ifdef CONFIG_BLUETOOTH_KEYBOARD */
#endif /* #ifndef MORSEBLUETOOTH_H_ */