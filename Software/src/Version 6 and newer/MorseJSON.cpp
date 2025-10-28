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

#include "MorseJSON.h"

///// create json output for serial port
using namespace MorseJSON;

void MorseJSON::jsonDevice(String brd, String vsn) {
	DynamicJsonDocument doc(256);
	JsonObject device = doc.createNestedObject("device");
	device["hardware"] = brd;
	device["firmware"] = vsn;
	device["protocol"] = M32P_VERSION;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonError(String errormessage) {
	MorseJSON::jsonCreate("error", errormessage, "");
}

void MorseJSON::jsonOK(void) {
	MorseJSON::jsonCreate("ok", "OK", "");
}

void MorseJSON::jsonMenu(String path, unsigned int number, boolean active, boolean exec) {
	DynamicJsonDocument doc(256);
	JsonObject obj = doc.createNestedObject("menu");
	obj["content"] = path;
	obj["menu number"] = number;
	obj["executable"] = exec;
	obj["active"] = active;

	serializeJson(doc, Serial);
}

void MorseJSON::jsonMenuList(void) { // get all parameter names and their values, and send them as json object
	DynamicJsonDocument doc(4000);
	DynamicJsonDocument doc2(4000);
	StaticJsonDocument<3000> arr;
	JsonArray array = arr.to<JsonArray>();

	for (uint8_t i = 1; i < menuN; ++i)
	{ // menu numbers start at 1
		JsonObject obj = doc.createNestedObject();
		obj["content"] = MorseMenu::getMenuPath(i);
		obj["menu number"] = i;
		obj["executable"] = MorseMenu::isRemotelyExecutable(i);
		array.add(obj);
	}
	doc["menus"] = array;
	doc2["menus"] = doc;
	serializeJson(doc2, Serial);
}

void MorseJSON::jsonParameter(String token) { /// find parameter "token" and create json object for it
	String pname;
	pname.reserve(20);
	bool found = false;
	for (uint8_t i = 0; i <= posSerialOut; ++i)
	{
		pname = MorsePreferences::pliste[i].parName;
		pname.toLowerCase();
		if (token != pname)
			continue;
		jsonConfigLong(MorsePreferences::pliste[i]);
		found = true;
		break;
	}
	// not found
	if (!found)
		MorseJSON::jsonError("INVALID PARAMETER");
}

void MorseJSON::jsonParameterList(void) { // get all parameter names and their values, and send them as json object
	DynamicJsonDocument doc(5200);
	DynamicJsonDocument doc2(5200);
	StaticJsonDocument<4096> arr;
	JsonArray array = arr.to<JsonArray>();

	for (uint8_t i = 0; i <= posSerialOut; ++i)
	{
		JsonObject obj = doc.createNestedObject();
		obj["name"] = MorsePreferences::pliste[i].parName;
		obj["value"] = (int)MorsePreferences::pliste[i].value;
		if (MorsePreferences::pliste[i].isMapped)
			obj["displayed"] = MorsePreferences::pliste[i].mapping[MorsePreferences::pliste[i].value];
		else if (i == posMaxSequence && MorsePreferences::pliste[i].value == 0)
			obj["displayed"] = "Unlimited";
		else
			obj["displayed"] = String(MorsePreferences::pliste[i].value);
		array.add(obj);
	}
	doc["configs"] = array;
	doc2["configs"] = doc;
	serializeJson(doc2, Serial);
}

void MorseJSON::jsonGetKoch(void) { // get current Koch lesson setting, and associated values
	DynamicJsonDocument doc(1536);
	StaticJsonDocument<1024> arr;
	JsonObject kochlesson = doc.createNestedObject("kochlesson");
	kochlesson["value"] = MorsePreferences::kochFilter;
	kochlesson["minimum"] = MorsePreferences::kochMinimum;
	kochlesson["maximum"] = MorsePreferences::kochMaximum;
	// int diff = MorsePreferences::kochMaximum - MorsePreferences::kochMinimum;
	JsonArray array = arr.to<JsonArray>();
	for (int i = MorsePreferences::kochMinimum - 1; i < MorsePreferences::kochMaximum; ++i)
	{
		String s = koch.getKochChar(i);
		array.add(cleanUpProSigns(s));
	}
	kochlesson["characters"] = array;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonConfigLong(MorsePreferences::parameter p) {
	DynamicJsonDocument doc(512);
	StaticJsonDocument<256> arr;
	JsonObject conf = doc.createNestedObject("config");
	conf["name"] = p.parName;
	conf["value"] = p.value;
	conf["description"] = p.parDescript;
	conf["minimum"] = p.minimum;
	conf["maximum"] = p.maximum;
	conf["step"] = p.stepValue;
	conf["isMapped"] = p.isMapped; //? "true" : "false";
	if (p.isMapped)
	{
		JsonArray array = arr.to<JsonArray>();
		for (int i = 0; i <= p.maximum; ++i)
		{
			array.add(p.mapping[i]);
		}
		conf["mapped values"] = array;
	}
	serializeJson(doc, Serial);
}

void MorseJSON::jsonConfigShort(String item, int value, String displayed) {
	DynamicJsonDocument doc(128);
	JsonObject conf = doc.createNestedObject("config");
	conf["name"] = item;
	conf["value"] = value;
	conf["displayed"] = displayed;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonCreate(String objName, String path, String state) {
	DynamicJsonDocument doc(256);
	JsonObject obj = doc.createNestedObject(objName);
	obj["content"] = path;
	if (state != "")
		obj["status"] = state;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonActivate(actMessage active) { /// if active == 1: we are LEAVING a mode
	const char *message[] = {"EXIT", "ON", "SET", "CANCELLED", "RECALLED", "CLEAjsonDevice"};
	DynamicJsonDocument doc(32);
	JsonObject activate = doc.createNestedObject("activate");
	if (active > 5)
		active = (actMessage)0;
	activate["state"] = message[active];
	serializeJson(doc, Serial);
}

void MorseJSON::jsonControl(String item, uint8_t value, uint8_t mini, uint8_t maxi, boolean detailed) {
	DynamicJsonDocument doc(256);
	JsonObject control = doc.createNestedObject("control");
	control["name"] = item;
	control["value"] = value;
	if (detailed)
	{
		control["minimum"] = mini;
		control["maximum"] = maxi;
	}
	serializeJson(doc, Serial);
}

void MorseJSON::jsonControls(void) {
	DynamicJsonDocument liste(128);

	JsonObject speedo = liste.createNestedObject();
	speedo["name"] = "speed";
	speedo["value"] = MorsePreferences::wpm;
	JsonObject volumeo = liste.createNestedObject();
	volumeo["name"] = "volume";
	volumeo["value"] = MorsePreferences::sidetoneVolume;

	DynamicJsonDocument doc(256);
	doc["controls"] = liste;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonSnapshots(void) {
	DynamicJsonDocument doc(164);
	StaticJsonDocument<164> arr;
	JsonObject conf = doc.createNestedObject("snapshots");
	JsonArray array = arr.to<JsonArray>();

	// DEBUG("memCounter: " + String(MorsePreferences::memCounter));
	for (int i = 0; i < MorsePreferences::memCounter; ++i)
	{
		array.add((int)MorsePreferences::memories[i] + 1);
	}
	conf["existing snapshots"] = array;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonFileStats(void) { // get info about SPIFFS file system
	long unsigned int total, used;

	DynamicJsonDocument doc(128);
	JsonObject conf = doc.createNestedObject("file");
	File file = SPIFFS.open("/player.txt", "r");
	conf["size"] = file.size();
	file.close();
	used = SPIFFS.usedBytes();
	total = SPIFFS.totalBytes();
	conf["free"] = total - used;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonFileFirstLine(void) {
	File file = SPIFFS.open("/player.txt", "r"); // Open the file for reading in SPIFFS - no error handling, file must exist
	Serial.print("{\"file\":{\"first line\":\"");
	while (file.available())
	{
		char c = file.read();
		if (c == '{' || c == '}')
			continue;
		if (c == '\n')
			break;
		else
			Serial.write(c);
	}
	Serial.print("\"}}");
	file.close();
}

void MorseJSON::jsonFileText(void) {												 // get file content as json object (file should not contain curly braces!)
	File file = SPIFFS.open("/player.txt", "r"); // Open the file for reading in SPIFFS - no error handling, file must exist
	Serial.print("{\"file\":{\"text\":\"");
	while (file.available())
	{
		char c = file.read();
		if (c == '{' || c == '}')
			continue;
		Serial.write(c);
	}
	Serial.print("\"}}");
	file.close();
}

void MorseJSON::jsonGetWifi(void) {
	DynamicJsonDocument liste(512);
	String activeWiFiConf = MorsePreferences::wlanSSID == "" ? "INVALID" : MorsePreferences::wlanSSID + MorsePreferences::wlanTRXPeer;

	JsonObject entry1 = liste.createNestedObject();
	entry1["ssid"] = MorsePreferences::wlanSSID1;
	entry1["trxpeer"] = MorsePreferences::wlanTRXPeer1;
	entry1["selected"] = (MorsePreferences::wlanSSID1 + MorsePreferences::wlanTRXPeer1 == activeWiFiConf ? true : false);

	JsonObject entry2 = liste.createNestedObject();
	entry2["ssid"] = MorsePreferences::wlanSSID2;
	entry2["trxpeer"] = MorsePreferences::wlanTRXPeer2;
	entry2["selected"] = (MorsePreferences::wlanSSID2 + MorsePreferences::wlanTRXPeer2 == activeWiFiConf ? true : false);

	JsonObject entry3 = liste.createNestedObject();
	entry3["ssid"] = MorsePreferences::wlanSSID3;
	entry3["trxpeer"] = MorsePreferences::wlanTRXPeer3;
	entry3["selected"] = (MorsePreferences::wlanSSID3 + MorsePreferences::wlanTRXPeer2 == activeWiFiConf ? true : false);

	DynamicJsonDocument doc(512);
	doc["wifi"] = liste;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonGetCwStores(void) {
	DynamicJsonDocument doc(164);
	StaticJsonDocument<164> arr;
	JsonObject conf = doc.createNestedObject("CW Memories");
	JsonArray array = arr.to<JsonArray>();

	for (int i = 0; i < 8; ++i)
	{
		if (MorsePreferences::cwMemMask & 1 << i)
			array.add(i + 1);
	}

	conf["cw memories in use"] = array;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonGetCwStore(String value) {
	int number = value.toInt();
	if (number < 1 || number > 8)
		return (MorseJSON::jsonError("INVALID CW MEMORY NUMBER"));
	// check cwMemMask if this memory number is active
	if ((MorsePreferences::cwMemMask & 1 << (number - 1)) == 0b0) // empty memory
		MorseJSON::jsonError("CW MEMORY " + value + " IS EMPTY");
	else
	{
		DynamicJsonDocument doc(256);
		JsonObject obj = doc.createNestedObject("CW Memory");
		obj["number"] = number;
		obj["content"] = String(MorsePreferences::cwMem[number - 1]);
		serializeJson(doc, Serial);
	}
}