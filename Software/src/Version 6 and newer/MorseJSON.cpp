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
#include "MorseOutput.h"   // getPowerpathState() for the battery charge state
#include "M32ProtocolOut.h" // m32out: delivers protocol output to every handshaken transport
#ifdef CONFIG_BLE_SERIAL
#include "MorseBleSerial.h" // txMakeRoom(): keeps a bulk reply whole on the BLE transport
#endif
#ifdef CONFIG_PRACTICE_STATS
#include "MorsePracticeStats.h"
#include <mbedtls/base64.h>
#endif
#ifdef CONFIG_CW_GAME
// GET game/scores: each game owns the layout of its own high-score table, so
// each exports its own rows rather than having that knowledge re-implemented
// here (CLAUDE.md §5).
#include "MorseGame.h"
#include "MorseMorsel.h"
#include "MorseGridScore.h"
#include "MorseMemoryChain.h"
#include "MorseRadioCave.h"
#endif

///// create json output for serial port
using namespace MorseJSON;

// Parameters per page of GET configs/details (protocol 1.4). Eight of them
// serialize to ~1.7 KB and occupy ~3.5 KB of document - the same order as the
// long-established GET configs, so no new memory ground is broken. Purely an
// implementation choice: the reply carries "count" and "more", so this can be
// retuned without touching any client.
#define CONFIG_DETAILS_PAGE 8

// Defined next to jsonConfigLong, its other caller (see the comment there).
static void fillConfigObject(JsonObject conf, const MorsePreferences::parameter &p);

// Chunked Print adapter: ArduinoJson's serializeJson(doc, Print&) emits one
// write() call per byte; through the tee that is one Serial.write AND one BLE
// txEnqueue call chain PER BYTE — thousands of them for a multi-KB GET
// response, in the same loop pass as the keyer. Collect ~256-byte chunks and
// hand those to m32out instead.
//
// Bulk mode is for the replies that stream unbounded data — the file listing,
// player.txt, the practice log. Those are many times the size of the 4 KB BLE
// TX ring, which without help fills up and drops the rest of the reply (torn
// JSON, client re-issues the GET). In bulk mode each chunk first waits, briefly,
// for the client to take what is already queued. A reply built from a
// JsonDocument is NOT bulk: those fit the ring, and jsonSend() is also on the
// keying path (a jsonControl while the operator is sending), where txEnqueue's
// never-wait rule holds.
namespace {
class ChunkedM32Out : public Print {
  public:
    explicit ChunkedM32Out(bool bulk = false) : bulk(bulk) {}
    ~ChunkedM32Out() { flushChunk(); }   // no emitter can end up truncated by a forgotten flush
    size_t write(uint8_t c) override {
        buf[n++] = c;
        if (n == sizeof(buf))
            flushChunk();
        return 1;
    }
    size_t write(const uint8_t *buffer, size_t size) override {
        for (size_t i = 0; i < size; ++i)
            write(buffer[i]);
        return size;
    }
    void flushChunk() {
        if (n) {
#ifdef CONFIG_BLE_SERIAL
            // One timeout is all the evidence needed that this client is not
            // keeping up: stop waiting for the remainder of THIS reply and let the
            // ring's own backoff drop it, rather than stalling the loop chunk after
            // chunk. A healthy link never gets here: making room for 256 bytes
            // takes ~100 ms even at a 20-byte MTU, well inside the 500.
            if (bulk && bleWait && !MorseBleSerial::txMakeRoom(n, 500))
                bleWait = false;
#endif
            m32out.write(buf, n);
            n = 0;
        }
    }
  private:
    const bool bulk;
    bool bleWait = true;                 // cleared once the BLE client has shown it cannot keep up
    uint8_t buf[256];
    size_t n = 0;
};
}

void MorseJSON::jsonSend(const JsonDocument& doc) {
    ChunkedM32Out out;
    serializeJson(doc, out);
    out.flushChunk();
}

void MorseJSON::jsonDevice(const String& brd, const String& vsn) { // create json object with device information, and send it to the serial output
	StaticJsonDocument<256> doc;
	JsonObject device = doc.createNestedObject("device");
	device["hardware"] = brd;
	device["firmware"] = vsn;
	device["protocol"] = M32P_VERSION;
	device["build"]    = COMPILEDATE;   // firmware compile date (__DATE__), additive build stamp
	// Which firmware edition is running. Both Pocket editions share one HW_NAME
	// ("M32 Pocket (Wroom)"), so "hardware" alone cannot tell a client whether it
	// is talking to the accessibility build — which it must know to offer the
	// right user manual. Additive: the protocol version stays 1.3, and older
	// firmware simply omits the property.
#ifdef CONFIG_AUDIO_A11Y
	device["edition"]  = "accessibility";
#else
	device["edition"]  = "standard";
#endif
	MorseJSON::jsonSend(doc);
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

	MorseJSON::jsonSend(doc);
}

void MorseJSON::jsonMenuList(void) { // get all parameter names and their values, and send them as json object
	// sized from menuN: 3 slots per entry plus the copied path string (paths run
	// up to ~45 chars). The old fixed 3072 silently truncated the list at ~34
	// entries once the games/QSO-bot menus grew menuN past 40 — ArduinoJson
	// drops adds on a full pool, so GET menus lost the whole WiFi Functions
	// block on BOTH variants (classic menuN = 47, TFT more; found on hardware
	// while remotely executing Disp MAC Addr — the full list arrives again).
	DynamicJsonDocument doc(JSON_ARRAY_SIZE(menuN) + menuN * (JSON_OBJECT_SIZE(3) + 64) + 128);
    JsonArray array = doc.createNestedArray("menus");
      for (uint8_t i = 1; i < menuN; ++i) {
          JsonObject obj = array.createNestedObject();
          obj["content"] = MorseMenu::getMenuPath(i);
          obj["menu number"] = i;
          obj["executable"] = MorseMenu::isRemotelyExecutable(i);
      }
    MorseJSON::jsonSend(doc);
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
       MorseJSON::jsonSend(doc);
}

// Protocol 1.4: paginated bulk read of the full parameter descriptions.
// Without it a client needs one round trip per parameter to build its
// preferences view - 48 of them, about 10 s over BLE. The firmware picks the
// page size and the client just follows "more" and "from"+"count", so the
// page can be retuned later without breaking any client.
//
// Order and membership are exactly those of GET configs, i.e. pliste[]
// 0..posSerialOut, so a client can index one sequence across both commands.
// (The preferences-MENU order, allOptions[], is deliberately NOT used: it also
// carries the action items - Call Sign, Op Name, Reset Scores, Practice Set -
// which have no pliste[] entry and are not parameters at all.)
void MorseJSON::jsonParameterDetails(uint8_t from) {
	if (from > posSerialOut) {                       // desynchronised client: fail loudly
		MorseJSON::jsonError("INVALID PARAMETER");
		return;
	}
	DynamicJsonDocument doc(4096);                   // same sizing as GET configs
	JsonObject det = doc.createNestedObject("configdetails");
	det["from"]  = from;
	det["count"] = 0;                                // both filled in once the page is built
	det["total"] = (int)posSerialOut + 1;            // build-dependent: advisory, "more" is the authority
	det["more"]  = false;
	JsonArray items = det.createNestedArray("items");
	uint8_t i = from;
	for (; i <= posSerialOut && (i - from) < CONFIG_DETAILS_PAGE; ++i) {
		// A fully-mapped parameter costs roughly 400 B of document. Stop before
		// the pool can overflow: ArduinoJson drops adds SILENTLY when it is
		// full, which is exactly how GET menus once lost its whole tail.
		if (i > from && doc.memoryUsage() + 420 > doc.capacity())
			break;
		fillConfigObject(items.createNestedObject(), MorsePreferences::pliste[i]);
	}
	det["count"] = i - from;
	det["more"]  = (i <= posSerialOut);
	MorseJSON::jsonSend(doc);
}

// Protocol 1.4: feature discovery. Everything documented for the reported
// protocol version is present EXCEPT the build-dependent commands - the games
// and the practice-statistics log are compiled out of some builds. Listing
// them here saves a client from probing and reading an error reply.
void MorseJSON::jsonCapabilities(void) {
	StaticJsonDocument<384> doc;
	JsonObject cap = doc.createNestedObject("capabilities");
	cap["protocol"] = M32P_VERSION;
	JsonArray feat = cap.createNestedArray("features");
	feat.add("configs/details");                     // 1.4 bulk parameter read
#ifdef CONFIG_CW_GAME
	feat.add("game/scores");                         // 1.4 game high-score read/clear
#endif
#ifdef CONFIG_PRACTICE_STATS
	feat.add("stats/log");
#endif
	MorseJSON::jsonSend(doc);
}

#ifdef CONFIG_CW_GAME
// Protocol 1.4: the game high-score tables, which were previously reachable
// only on the device itself. Read-only — PUT game/scores/clear wipes them, but
// nothing writes a score back over the wire.
//
// The row FIELDS differ per game, because the games score differently (time,
// effective speed, chain length, ...); "id" is the stable machine name and
// "name" the display one. Radio Cave keeps no table at all, only a saved game.
void MorseJSON::jsonGameScores(void) {
	// Worst case is ~4.4 KB of document with every table full; 6 KB leaves
	// headroom and still stays under the long-standing GET menus allocation.
	DynamicJsonDocument doc(6144);
	JsonObject root = doc.createNestedObject("gamescores");
	JsonArray games = root.createNestedArray("games");

	JsonObject g = games.createNestedObject();
	g["id"] = "invaders";  g["name"] = "Morse Invaders";
	MorseGame::exportHighScores(g.createNestedArray("scores"));

	g = games.createNestedObject();
	g["id"] = "morsel";    g["name"] = "Morsel";
	MorseMorsel::exportHighScores(g.createNestedArray("scores"));

	g = games.createNestedObject();
	g["id"] = "trailblazer"; g["name"] = "Trailblazer";
	MorseGridScore::exportHighScores(MorseGridScore::TRAILBLAZER, g.createNestedArray("scores"));

	g = games.createNestedObject();
	g["id"] = "foxhunt";   g["name"] = "Fox Hunt";
	MorseGridScore::exportHighScores(MorseGridScore::FOXHUNT, g.createNestedArray("scores"));

	g = games.createNestedObject();
	g["id"] = "memorychain-chars"; g["name"] = "Memory Chain (Characters)";
	MorseMemoryChain::exportHighScores(MorseMemoryChain::CHARACTERS, g.createNestedArray("scores"));

	g = games.createNestedObject();
	g["id"] = "memorychain-calls"; g["name"] = "Memory Chain (Call Signs)";
	MorseMemoryChain::exportHighScores(MorseMemoryChain::CALLSIGNS, g.createNestedArray("scores"));

	g = games.createNestedObject();
	g["id"] = "radiocave"; g["name"] = "Radio Cave";
	g.createNestedArray("scores");                   // no score table, only a save
	g["saved"] = MorseRadioCave::hasSavedGame();

	if (doc.overflowed()) {                          // never ship a silently truncated table
		MorseJSON::jsonError("GAME SCORES TOO LARGE");
		return;
	}
	MorseJSON::jsonSend(doc);
}
#endif  // CONFIG_CW_GAME

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
	MorseJSON::jsonSend(doc);
}

// The single truth about how one parameter is represented on the wire: filled
// in by GET config/<name> (jsonConfigLong) and, unchanged, by every item of
// the 1.4 bulk read (jsonParameterDetails). Both go through here so the two
// commands can never drift apart. All the strings are const char* literals
// from pliste[], so ArduinoJson stores pointers to them and copies nothing.
static void fillConfigObject(JsonObject conf, const MorsePreferences::parameter &p) {
	conf["name"] = p.parName;
	conf["value"] = p.value;
	conf["description"] = p.parDescript;
	conf["minimum"] = p.minimum;
	conf["maximum"] = p.maximum;
	conf["step"] = p.stepValue;
	conf["isMapped"] = p.isMapped; //? "true" : "false";
	if (p.isMapped)
	{
		JsonArray array = conf.createNestedArray("mapped values");
		for (int i = 0; i <= p.maximum; ++i)
		{
			array.add(p.mapping[i]);
		}
	}
}

void MorseJSON::jsonConfigLong(MorsePreferences::parameter p) {
	StaticJsonDocument<512> doc;
	JsonObject conf = doc.createNestedObject("config");
	fillConfigObject(conf, p);
	MorseJSON::jsonSend(doc);
}

void MorseJSON::jsonConfigShort(const String& item, int value, const String& displayed) { // create json object for a parameter with its value, and send it to the serial output
	StaticJsonDocument<128> doc;
	JsonObject conf = doc.createNestedObject("config");
	conf["name"] = item;
	conf["value"] = value;
	conf["displayed"] = displayed;
	MorseJSON::jsonSend(doc);
}

void MorseJSON::jsonCreate(const String& objName, const String& path, const String& state) { // create json object with name "objName", and two properties "content" and "state", and send it to the serial output
	StaticJsonDocument<256> doc;
	JsonObject obj = doc.createNestedObject(objName);
	obj["content"] = path;
	if (state != "")
		obj["status"] = state;
	MorseJSON::jsonSend(doc);
}

void MorseJSON::jsonActivate(actMessage active) { /// if active == 1: we are LEAVING a mode
	const char *message[] = {"EXIT", "ON", "SET", "CANCELLED", "RECALLED", "CLEARED"};
	StaticJsonDocument<64> doc;  // 32 is too small for ArduinoJson overhead
	JsonObject activate = doc.createNestedObject("activate");
	if (active > 5)
		active = (actMessage)0;
	activate["state"] = message[active];
	MorseJSON::jsonSend(doc);
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
	MorseJSON::jsonSend(doc);
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
	MorseJSON::jsonSend(doc);
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
	MorseJSON::jsonSend(doc);
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
	MorseJSON::jsonSend(doc);
}

void MorseJSON::jsonFileFirstLine(void) {
	ChunkedM32Out out(true);            // bulk: a player.txt without a newline IS its first line
	File file = SPIFFS.open("/player.txt", "r"); // Open the file for reading in SPIFFS - no error handling, file must exist
	out.print("{\"file\":{\"first line\":\"");
	while (file.available())
	{
		char c = file.read();
		// The line ends the reply here instead of being escaped - that is the
		// only intended difference from jsonFileText() below. Everything else
		// has to follow its escape-and-strip-braces scheme: a quote or a
		// backslash in the first line used to go out raw and tear the JSON
		// apart, and so did a lone CR from a CRLF file.
		if (c == '\n' || c == '\r')
			break;
		switch (c) {
			case '"':  out.print("\\\""); break;
			case '\\': out.print("\\\\"); break;
			case '\t': out.print("\\t");  break;
			case '{':  break;  // skip curly braces
			case '}':  break;
			default:
				// Control characters go, UTF-8 continuation bytes (>= 0x80) stay.
				// char is unsigned on the Xtensa toolchain, so the cast changes
				// nothing today - it just says which of the two is meant.
				if ((uint8_t) c >= 0x20)
					out.write(c);
				break;
		}
	}
	out.print("\"}}");
	out.flushChunk();
	file.close();
}

void MorseJSON::jsonFileText(void) {
	ChunkedM32Out out(true);            // bulk: player.txt is as long as the user made it
	File file = SPIFFS.open("/player.txt", "r");
	out.print("{\"file\":{\"text\":\"");
	while (file.available())
	{
		char c = file.read();
		switch (c) {
			case '"':  out.print("\\\""); break;
			case '\\': out.print("\\\\"); break;
			case '\n': out.print("\\n");  break;
			case '\r': out.print("\\r");  break;
			case '\t': out.print("\\t");  break;
			case '{':  break;  // skip curly braces
			case '}':  break;
			default:
				if (c >= 0x20)   // skip other control characters
					out.write(c);
				break;
		}
	}
	out.print("\"}}");
	out.flushChunk();
	file.close();
}

#ifdef CONFIG_PRACTICE_STATS
void MorseJSON::jsonStatsLog(void) {
	// Unlike the WiFi stats page (only reachable by first going through the
	// top-level menu, which already flushes any open segment via
	// MorseMenu::menuExec()'s endSegment() call), this is read over USB/serial
	// and so can be checked while the device is still sitting in an active
	// Koch practice mode. Flush first so the round just completed is included
	// instead of silently missing until the user formally exits that mode.
	MorsePracticeStats::endSegment();

	// Base64, not the escape-and-strip-braces scheme jsonFileText() uses: the
	// M32 config tool's serial reader (waitForResponse() in m32_config_tool.html)
	// finds the response by counting '{'/'}' in the raw stream, so a raw JSONL
	// payload (which is full of '{'/'}') would desync that parser. Streamed
	// out through the chunker in ~256-byte pieces — never holds the whole file
	// in RAM, same reasoning as jsonFileText() reading byte-by-byte.
	ChunkedM32Out out(true);            // bulk: the whole practice log, base64
	out.print("{\"stats\":{\"log\":\"");
	if (SPIFFS.exists(MorsePracticeStats::logPath)) {
		File file = SPIFFS.open(MorsePracticeStats::logPath, "r");
		unsigned char inBuf[48];
		unsigned char outBuf[96];
		size_t n;
		while ((n = file.read(inBuf, sizeof(inBuf))) > 0) {
			size_t outLen = 0;
			mbedtls_base64_encode(outBuf, sizeof(outBuf), &outLen, inBuf, n);
			out.write(outBuf, outLen);
		}
		file.close();
	}
	out.print("\",\"used\":");
	out.print(MorsePracticeStats::usedBytes());
	out.print(",\"total\":");
	out.print(MorsePracticeStats::totalBytes());
	out.print(",\"logSize\":");
	out.print(MorsePracticeStats::logBytes());
	out.print(",\"enabled\":");
	out.print(MorsePracticeStats::enabled() ? "true" : "false");
	out.print("}}");
	out.flushChunk();
}
#endif

void MorseJSON::jsonFilePart(const String& name, uint8_t index, uint8_t total) {
    StaticJsonDocument<192> doc;
    JsonObject obj = doc.createNestedObject("filepart");
    obj["name"] = name;
    obj["selected"] = index + 1;    // 1-based for the serial client
    obj["count"] = total;
    MorseJSON::jsonSend(doc);
}

// File names come out of the flash, and PUT file/begin accepts whatever name a
// client sends, so a quote or a backslash can end up in one. ArduinoJson used to
// escape those for us; a streamed reply has to do it itself, or one odd name
// breaks the JSON for everything after it.
static void printJsonEscaped(Print &out, const char *s) {
    for (; *s; ++s) {
        if (*s == '"' || *s == '\\') {
            out.print('\\');
            out.print(*s);
        } else if ((uint8_t) *s < 0x20) {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\u%04x", *s);
            out.print(esc);
        } else
            out.print(*s);
    }
}

// Streamed rather than serialized from a JsonDocument: the Accessibility Edition
// keeps ~500 speech clips in /voice/, and listing them is ~20 KB. ArduinoJson
// fails SILENTLY once its document is full, so the old 2 KB document stopped
// after some 30 files, dropped the size of the entry that straddled the end (it
// reached the client as "NaN KB") and lost the total/used/free trailer with it.
// No fixed size is right for a flash whose contents the user chooses, so this
// streams the way jsonFileText() and jsonStatsLog() do - through the 256-byte
// chunker, because a per-byte tee write costs one Serial.write AND one BLE
// enqueue each. The reply shape is unchanged.
void MorseJSON::jsonFileList(void) {
    ChunkedM32Out out(true);            // bulk: ~20 KB on the Accessibility Edition
    out.print("{\"files\":[");

    File root = SPIFFS.open("/");
    File f = root.openNextFile();
    bool first = true;
    while (f) {
        out.print(first ? "{\"name\":\"" : ",{\"name\":\"");
        first = false;
        printJsonEscaped(out, f.path());    // f.name() would strip the directory
        out.print("\",\"size\":");
        out.print(f.size());
        out.print('}');
        f = root.openNextFile();
    }

    out.print("],\"total\":");
    out.print(SPIFFS.totalBytes());
    out.print(",\"used\":");
    out.print(SPIFFS.usedBytes());
    out.print(",\"free\":");
    out.print(SPIFFS.totalBytes() - SPIFFS.usedBytes());
    out.print('}');
    out.flushChunk();
}

void MorseJSON::jsonUploadComplete(const String& filename, uint32_t size) {
    StaticJsonDocument<128> doc;
    JsonObject obj = doc.createNestedObject("upload");
    obj["file"] = filename;    // this one is fine — we pass the filename ourselves
    obj["size"] = size;
    MorseJSON::jsonSend(doc);
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

	MorseJSON::jsonSend(doc);
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
	MorseJSON::jsonSend(doc);
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
		MorseJSON::jsonSend(doc);
	}
}

// ============================================================================
// Protocol v1.3 extensions
// ============================================================================

void MorseJSON::jsonGetSnapshot(uint8_t snapNumber) {
    // Read snapshot contents without recalling (non-destructive read)
    // snapNumber is 0-based (0..7), corresponding to snap0..snap7.
    // decodeSnapshot() understands both the blob format and legacy
    // per-key snapshots written by older firmware.
    String snapname = "snap" + String(snapNumber);
    uint8_t vals[posSerialOut];
    uint8_t lastExec, kochLen, useCustom;
    String customSet;
    if (!MorsePreferences::decodeSnapshot(snapname.c_str(), vals, lastExec, kochLen, useCustom, customSet)) {
        MorseJSON::jsonError("Cannot open snapshot " + String(snapNumber + 1));
        return;
    }

    DynamicJsonDocument doc(3072);
    JsonObject snap = doc.createNestedObject("snapshot");
    snap["number"] = snapNumber + 1;  // report as 1-based to match user-facing numbering

    snap["lastExecuted"] = lastExec;
    if (lastExec > 0 && lastExec < menuN)
        snap["menuName"] = MorseMenu::getMenuPath(lastExec);
    else
        snap["menuName"] = "—";

    // Custom chars stored in snapshots
    JsonObject custom = snap.createNestedObject("customChars");
    custom["active"] = (useCustom != 0);
    custom["characters"] = customSet;

    // All pliste[] parameters that snapshots actually contain (training settings;
    // device/hardware/game settings are excluded — see storedInSnapshot())
    JsonArray configs = snap.createNestedArray("configs");
    for (uint8_t i = 0; i < posSerialOut; ++i) {
        if (!MorsePreferences::storedInSnapshot((prefPos) i))
            continue;  // not stored in snapshots (also hides stale keys in old snapshots)
        uint8_t val = vals[i];
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

    MorseJSON::jsonSend(doc);
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
    MorseJSON::jsonSend(doc);
}

void MorseJSON::jsonGetCustomChars(void) {
    StaticJsonDocument<256> doc;
    JsonObject obj = doc.createNestedObject("customchars");
    obj["active"] = MorsePreferences::useCustomChars;
    obj["characters"] = MorsePreferences::customCharSet;
    MorseJSON::jsonSend(doc);
}

void MorseJSON::jsonGetPracticeChars(void) {
    StaticJsonDocument<192> doc;
    JsonObject obj = doc.createNestedObject("practicechars");
    obj["characters"] = MorsePreferences::practiceCharSet;
    serializeJson(doc, Serial);
}

void MorseJSON::jsonGetHardware(void) {
    StaticJsonDocument<256> doc;
    JsonObject hw = doc.createNestedObject("hardware");
    hw["brightness"] = MorsePreferences::oledBrightness;
    hw["leftHanded"] = MorsePreferences::leftHanded;
#ifdef CONFIG_CN3_PADDLE
    hw["cn3Paddle"] = MorsePreferences::cn3Mechanical ? "mechanical" : "touch";
#endif
    hw["vAdjust"] = MorsePreferences::vAdjust;
#ifndef LORA_DISABLED
    hw["loraBand"] = MorsePreferences::loraBand;
    hw["loraFrequency"] = MorsePreferences::loraQRG;
    hw["loraPower"] = MorsePreferences::loraPower;
#endif
    MorseJSON::jsonSend(doc);
}

void MorseJSON::jsonGetBattery(void) {
    StaticJsonDocument<128> doc;
    JsonObject bat = doc.createNestedObject("battery");
#ifdef CONFIG_MCP73871
    bat["voltage"] = batteryVoltage();
    // Charge state from the MCP73871 status pins — the same source the device's own
    // battery icon uses. A voltage threshold is unreliable: while charging over USB the
    // cell voltage already sits near 4.2 V long before the charger reports "complete".
    //
    // Bit 0 is PG (power good), LOW while an external supply is present -- every
    // mapped state below has it low. HIGH means there is no supply and the device
    // is running off the cell. That case used to fall into the "charging"
    // default, which was a safe assumption only while this protocol required a
    // USB cable. Over BLE it is not: a phone talking to a device on battery was
    // told "USB - charging" (seen on hardware, M32 Pocket, 2026-08-20).
    uint8_t pp = MorseOutput::getPowerpathState();
    if (pp & 1) {
        bat["status"] = "battery";                      // no external supply: on the cell
    } else switch (pp) {
        case 4:  bat["status"] = "full";        break;   // charge complete
        case 6:  bat["status"] = "no battery";  break;   // running on USB, no cell installed
        case 2:
        default: bat["status"] = "charging";    break;   // charging
    }
#else
    // No charge controller to ask on this hardware, and no PG pin: a classic M32
    // running on battery cannot be told apart from one on USB, so this stays as
    // it was rather than guessing.
    bat["status"] = "usb powered";
#endif
    MorseJSON::jsonSend(doc);
}