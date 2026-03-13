#ifndef MORSEJSON_H_
#define MORSEJSON_H_

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

#include "MorsePreferences.h"

namespace MorseJSON
{
	void jsonDevice(const String& brd, const String& vsn);
	void jsonError(const String& errormessage);
	void jsonOK(void);
	void jsonMenu(const String& path, unsigned int number, boolean active, boolean exec);
	void jsonMenuList(void);
	void jsonParameter(const String& token);
	void jsonParameterList(void);
	void jsonGetKoch(void);
	void jsonConfigLong(MorsePreferences::parameter p);
	void jsonConfigShort(const String& item, int value, const String& displayed);
	void jsonCreate(const String& objName, const String& path, const String& state);
	void jsonActivate(actMessage active);
	void jsonControl(const String& item, uint8_t value, uint8_t mini, uint8_t maxi, boolean detailed);
	void jsonControls(void);
	void jsonSnapshots(void);
	void jsonFileStats(void);
	void jsonFileFirstLine(void);
	void jsonFileText(void);
	void jsonGetWifi(void);
	void jsonGetCwStores(void);
	void jsonGetCwStore(const String& value);
};

#endif /* #ifndef MORSEJSON_H_ */