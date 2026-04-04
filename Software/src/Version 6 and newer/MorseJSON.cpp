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
#include "MorseMenu.h"

///// create json output for serial port
using namespace MorseJSON;

void MorseJSON::jsonDevice(const String& brd, const String& vsn) { // create json object with device information, and send it to the serial output
	StaticJsonDocument<256> doc;
	JsonObject device = doc.createNestedObject("device");
	device["hardware"] = brd;
	device["firmware"] = vsn;
	device["protocol"] = M32P_VERSION;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonError(const String& errormessage) { // create json object with error message, and send it to the serial output
	MorseJSON::jsonCreate("error", errormessage, "");
}

void MorseJSON::jsonOK(void) {
	MorseJSON::jsonCreate("ok", "OK", "");
}

void MorseJSON::jsonMenu(const String& path, unsigned int number, boolean active, boolean exec) { // create json object for a menu item, and send it to the serial output
	StaticJsonDocument<256> doc;
	JsonObject obj = doc.createNestedObject("menu");
	obj["content"] = path;
	obj["menu number"] = number;
	obj["executable"] = exec;
	obj["active"] = active;

	serializeJson(doc, Serial);
}

void MorseJSON::jsonMenuList(void) { // get all parameter names and their values, and send them as json object
	DynamicJsonDocument doc(3072);   // one doc instead of two
    JsonArray array = doc.createNestedArray("menus");
      for (uint8_t i = 1; i < menuN; ++i) {
          JsonObject obj = array.createNestedObject();
          obj["content"] = MorseMenu::getMenuPath(i);
          obj["menu number"] = i;
          obj["executable"] = MorseMenu::isRemotelyExecutable(i);
      }
    serializeJson(doc, Serial);
}

void MorseJSON::jsonParameter(const String& token) { /// find parameter "token" and create json object for it
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
	DynamicJsonDocument doc(4096);
    JsonArray array = doc.createNestedArray("configs");
       for (uint8_t i = 0; i <= posSerialOut; ++i) {
           JsonObject obj = array.createNestedObject();
           obj["name"] = MorsePreferences::pliste[i].parName;
           obj["value"] = (int)MorsePreferences::pliste[i].value;
           if (MorsePreferences::pliste[i].isMapped)
               obj["displayed"] = MorsePreferences::pliste[i].mapping[MorsePreferences::pliste[i].value];
           else if (i == posMaxSequence && MorsePreferences::pliste[i].value == 0)
               obj["displayed"] = "Unlimited";
           else
               obj["displayed"] = String(MorsePreferences::pliste[i].value);
       }
       serializeJson(doc, Serial);
}

void MorseJSON::jsonGetKoch(void) { // get current Koch lesson setting, and associated values
	StaticJsonDocument<1536> doc;
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
	StaticJsonDocument<512> doc;
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

void MorseJSON::jsonConfigShort(const String& item, int value, const String& displayed) { // create json object for a parameter with its value, and send it to the serial output
	StaticJsonDocument<128> doc;
	JsonObject conf = doc.createNestedObject("config");
	conf["name"] = item;
	conf["value"] = value;
	conf["displayed"] = displayed;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonCreate(const String& objName, const String& path, const String& state) { // create json object with name "objName", and two properties "content" and "state", and send it to the serial output
	StaticJsonDocument<256> doc;
	JsonObject obj = doc.createNestedObject(objName);
	obj["content"] = path;
	if (state != "")
		obj["status"] = state;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonActivate(actMessage active) { /// if active == 1: we are LEAVING a mode
	const char *message[] = {"EXIT", "ON", "SET", "CANCELLED", "RECALLED", "CLEAjsonDevice"};
	StaticJsonDocument<64> doc;  // 32 is too small for ArduinoJson overhead
	JsonObject activate = doc.createNestedObject("activate");
	if (active > 5)
		active = (actMessage)0;
	activate["state"] = message[active];
	serializeJson(doc, Serial);
}

void MorseJSON::jsonControl(const String& item, uint8_t value, uint8_t mini, uint8_t maxi, boolean detailed) { /// create json object for a control item with its value, and send it to the serial output; if detailed == true, also include minimum and maximum values
	StaticJsonDocument<256> doc;
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
	StaticJsonDocument<128> liste;

	JsonObject speedo = liste.createNestedObject();
	speedo["name"] = "speed";
	speedo["value"] = MorsePreferences::wpm;
	JsonObject volumeo = liste.createNestedObject();
	volumeo["name"] = "volume";
	volumeo["value"] = MorsePreferences::sidetoneVolume;

	StaticJsonDocument<256> doc;
	doc["controls"] = liste;
	serializeJson(doc, Serial);
}

void MorseJSON::jsonSnapshots(void) {
	StaticJsonDocument<192> doc;
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

	StaticJsonDocument<128> doc;
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

void MorseJSON::jsonFilePart(const String& name, uint8_t index, uint8_t total) {
    StaticJsonDocument<192> doc;
    JsonObject obj = doc.createNestedObject("filepart");
    obj["name"] = name;
    obj["selected"] = index + 1;    // 1-based for the serial client
    obj["count"] = total;
    serializeJson(doc, Serial);
}

void MorseJSON::jsonFileList(void) {
    DynamicJsonDocument doc(2048);
    JsonArray array = doc.createNestedArray("files");

    File root = SPIFFS.open("/");
    File f = root.openNextFile();
    while (f) {
        JsonObject entry = array.createNestedObject();
        entry["name"] = String(f.path());    // ← was f.name(), which strips the directory
        entry["size"] = f.size();
        f = root.openNextFile();
    }
    doc["total"] = SPIFFS.totalBytes();
    doc["used"] = SPIFFS.usedBytes();
    doc["free"] = SPIFFS.totalBytes() - SPIFFS.usedBytes();
    serializeJson(doc, Serial);
}

void MorseJSON::jsonUploadComplete(const String& filename, uint32_t size) {
    StaticJsonDocument<128> doc;
    JsonObject obj = doc.createNestedObject("upload");
    obj["file"] = filename;    // this one is fine — we pass the filename ourselves
    obj["size"] = size;
    serializeJson(doc, Serial);
}

void MorseJSON::jsonGetWifi(void) {
	StaticJsonDocument<640> doc;
	JsonArray liste = doc.createNestedArray("wifi");

	String activeWiFiConf = MorsePreferences::wlanSSID == "" ? "INVALID" : MorsePreferences::wlanSSID + MorsePreferences::wlanTRXPeer;

	JsonObject entry1 = liste.createNestedObject();
	entry1["ssid"] = MorsePreferences::wlanSSID1;
	entry1["trxpeer"] = MorsePreferences::wlanTRXPeer1;
	entry1["selected"] = (!MorsePreferences::useEspNow && MorsePreferences::wlanSSID1 + MorsePreferences::wlanTRXPeer1 == activeWiFiConf);

	JsonObject entry2 = liste.createNestedObject();
	entry2["ssid"] = MorsePreferences::wlanSSID2;
	entry2["trxpeer"] = MorsePreferences::wlanTRXPeer2;
	entry2["selected"] = (!MorsePreferences::useEspNow && MorsePreferences::wlanSSID2 + MorsePreferences::wlanTRXPeer2 == activeWiFiConf);

	JsonObject entry3 = liste.createNestedObject();
	entry3["ssid"] = MorsePreferences::wlanSSID3;
	entry3["trxpeer"] = MorsePreferences::wlanTRXPeer3;
	entry3["selected"] = (!MorsePreferences::useEspNow && MorsePreferences::wlanSSID3 + MorsePreferences::wlanTRXPeer3 == activeWiFiConf);

	doc["espnow"] = MorsePreferences::useEspNow;
	doc["wlanChoice"] = MorsePreferences::wlanChoice;

	serializeJson(doc, Serial);
}

void MorseJSON::jsonGetCwStores(void) {
	StaticJsonDocument<192> doc;
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

void MorseJSON::jsonGetCwStore(const String& value) { // get content of CW memory "value" (number between 1 and 8), and send it as json object; if value is invalid, send json object with error message
	int number = value.toInt();
	if (number < 1 || number > 8)
		return (MorseJSON::jsonError("INVALID CW MEMORY NUMBER"));
	// check cwMemMask if this memory number is active
	if ((MorsePreferences::cwMemMask & 1 << (number - 1)) == 0b0) // empty memory
		MorseJSON::jsonError("CW MEMORY " + value + " IS EMPTY");
	else
	{
		StaticJsonDocument<256> doc;
		JsonObject obj = doc.createNestedObject("CW Memory");
		obj["number"] = number;
		obj["content"] = String(MorsePreferences::cwMem[number - 1]);
		serializeJson(doc, Serial);
	}
}

// ============================================================================
// Protocol v1.3 extensions
// ============================================================================

void MorseJSON::jsonGetSnapshot(uint8_t snapNumber) {
    // Read snapshot contents without recalling (non-destructive read)
    // snapNumber is 0-based (0..7), corresponding to snap0..snap7
    String snapname = "snap" + String(snapNumber);
    Preferences snapPref;
    if (!snapPref.begin(snapname.c_str(), true)) {  // open read-only
        MorseJSON::jsonError("Cannot open snapshot " + String(snapNumber + 1));
        return;
    }

    DynamicJsonDocument doc(3072);
    JsonObject snap = doc.createNestedObject("snapshot");
    snap["number"] = snapNumber + 1;  // report as 1-based to match user-facing numbering

    uint8_t lastExec = snapPref.getUChar("lastExecuted", 0);
    snap["lastExecuted"] = lastExec;
    if (lastExec > 0 && lastExec < menuN)
        snap["menuName"] = MorseMenu::getMenuPath(lastExec);
    else
        snap["menuName"] = "—";

    // Custom chars stored in snapshots
    JsonObject custom = snap.createNestedObject("customChars");
    custom["active"] = snapPref.getBool("useCustomChar", false);
    custom["characters"] = snapPref.getString("customCharSet", "");

    // All pliste[] parameters (except posTimeOut and posSerialOut which are excluded from snapshots)
    JsonArray configs = snap.createNestedArray("configs");
    for (uint8_t i = 0; i < posSerialOut; ++i) {
        if (i == posTimeOut)
            continue;  // not stored in snapshots
        uint8_t val = snapPref.getUChar(prefName[i], 255);
        if (val == 255)
            continue;  // not present in this snapshot
        JsonObject entry = configs.createNestedObject();
        entry["name"] = MorsePreferences::pliste[i].parName;
        entry["value"] = val;
        if (MorsePreferences::pliste[i].isMapped && val <= MorsePreferences::pliste[i].maximum)
            entry["displayed"] = MorsePreferences::pliste[i].mapping[val];
        else if (i == posMaxSequence && val == 0)
            entry["displayed"] = "Unlimited";
        else
            entry["displayed"] = String(val);
    }

    snapPref.end();
    serializeJson(doc, Serial);
}

void MorseJSON::jsonGetPlayer(void) {
    Preferences playerPref;
    playerPref.begin("morserino", true);  // read-only
    String call = playerPref.getString("playerCall", "");
    String name = playerPref.getString("playerName", "");
    playerPref.end();

    StaticJsonDocument<192> doc;
    JsonObject player = doc.createNestedObject("player");
    player["call"] = call;
    player["name"] = name;
    serializeJson(doc, Serial);
}

void MorseJSON::jsonGetCustomChars(void) {
    StaticJsonDocument<256> doc;
    JsonObject obj = doc.createNestedObject("customchars");
    obj["active"] = MorsePreferences::useCustomChars;
    obj["characters"] = MorsePreferences::customCharSet;
    serializeJson(doc, Serial);
}

void MorseJSON::jsonGetHardware(void) {
    StaticJsonDocument<256> doc;
    JsonObject hw = doc.createNestedObject("hardware");
    hw["brightness"] = MorsePreferences::oledBrightness;
    hw["leftHanded"] = MorsePreferences::leftHanded;
    hw["vAdjust"] = MorsePreferences::vAdjust;
#ifndef LORA_DISABLED
    hw["loraBand"] = MorsePreferences::loraBand;
    hw["loraFrequency"] = MorsePreferences::loraQRG;
    hw["loraPower"] = MorsePreferences::loraPower;
#endif
    serializeJson(doc, Serial);
}

void MorseJSON::jsonGetBattery(void) {
    StaticJsonDocument<128> doc;
    JsonObject bat = doc.createNestedObject("battery");
#ifdef CONFIG_MCP73871
    int16_t v = batteryVoltage();
    bat["voltage"] = v;
    bat["status"] = (v > 4100) ? "full" : "charging";
#else
    bat["status"] = "usb powered";
#endif
    serializeJson(doc, Serial);
}