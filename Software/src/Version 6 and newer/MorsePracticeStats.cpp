/******************************************************************************************************************************
 *  Practice Stats — implementation (see header for scope).
 *****************************************************************************************************************************/

#include "MorsePracticeStats.h"

#ifdef CONFIG_PRACTICE_STATS

#include "morsedefs.h"
#include "MorsePreferences.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

namespace MorsePracticeStats {

const char* const logPath = "/stats.jsonl";
static const char* const tmpPath = "/stats.jsonl.tmp";

// Cap kept well below the 1.5 MB SPIFFS partition, and small enough that the
// line-by-line trim below never needs more than one line in RAM at a time.
static const size_t MAX_LOG_BYTES = 128 * 1024;

// Matches the largest Koch character set (Koch::kochCharSet, MorsePreferences.h)
// — a segment can never legitimately touch more distinct characters than that.
static const uint8_t MAX_LESSON_CHARS = 51;

struct CharStat {
    char     c;
    uint16_t heard;      // presented at all (both listen and send)
    uint16_t attempts;   // sent (send/echo mode only)
    uint16_t errors;
};

struct Segment {
    bool          active;
    uint8_t       lesson;
    char          mode[16];
    unsigned long startMillis;
    CharStat      chars[MAX_LESSON_CHARS];
    uint8_t       numChars;
    uint16_t      words;      // count of distinct words/groups presented (both listen and send)
    uint32_t      totalChars; // sum of presented word lengths (both listen and send)
};

static Segment segment = { false, 0, "", 0, {}, 0, 0, 0 };
static int8_t  enabledCache = -1;   // -1 = not yet read from NVS

bool enabled() {
    if (enabledCache < 0) {
        Preferences p;
        p.begin("morserino", true);
        enabledCache = p.getBool("statsOn", true) ? 1 : 0;
        p.end();
    }
    return enabledCache == 1;
}

void setEnabled(bool on) {
    enabledCache = on ? 1 : 0;
    Preferences p;
    p.begin("morserino", false);
    p.putBool("statsOn", on);
    p.end();
    if (!on)
        endSegment();
}

bool hasWallClock() {
    time_t now;
    time(&now);
    return now > 1700000000;   // sanity floor (~2023-11); default epoch is 0
}

static void backfillTimestamps();

void setWallClock(time_t epochSeconds) {
    if (epochSeconds <= 0)
        return;
    bool wasSynced = hasWallClock();
    struct timeval tv = { epochSeconds, 0 };
    settimeofday(&tv, nullptr);
    if (!wasSynced)
        backfillTimestamps();
}

void tryNtpSync() {
    if (hasWallClock())
        return;
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");   // UTC — only the epoch matters, not local time
    unsigned long deadline = millis() + 4000;
    while (millis() < deadline) {
        if (hasWallClock()) {
            backfillTimestamps();   // configTime()'s SNTP client calls settimeofday() directly,
            return;                 // bypassing setWallClock() — so this path needs its own trigger.
        }
        delay(200);
    }
    // No internet on this network (or NTP blocked) — fine, the browser-based
    // /api/time sync (POST from the stats page) still covers this visit.
}

// Retroactively date any already-logged records (ts==0) that were flushed
// earlier in this same boot — e.g. "practice now, check the stats page a
// minute later" — so the wall clock only needing to arrive *eventually*,
// not before every segment, still gets you real dates for that session.
// Each record's "m" field is the raw millis() at flush time; if millis() now
// is still >= that value, no reboot/deep-sleep has happened since (millis()
// resets to 0 on wake, unlike the RTC-backed wall clock — see devdocs), so
// elapsed time since that record is a reliable millis() difference. Records
// from a prior boot (no "m" field, or millis() has since wrapped past it)
// are left alone — permanently undated, as already documented.
static void backfillTimestamps() {
    if (!SPIFFS.exists(logPath))
        return;
    time_t now;
    time(&now);
    unsigned long nowMillis = millis();

    File src = SPIFFS.open(logPath, "r");
    File dst = SPIFFS.open(tmpPath, "w");
    if (!src || !dst)
        return;

    bool changedAny = false;
    while (src.available()) {
        String line = src.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;
        // ArduinoJson's own rule of thumb: ~2x the minified JSON text size is
        // enough pool for parsing it (records can now be a few KB for a
        // fully-learned Listen segment with many distinct characters heard).
        DynamicJsonDocument doc(128 + line.length() * 2);
        if (deserializeJson(doc, line) == DeserializationError::Ok) {
            if (doc["ts"] == 0 && doc.containsKey("m")) {
                unsigned long recordMillis = doc["m"];
                if (nowMillis >= recordMillis) {
                    time_t backfilled = now - (time_t)((nowMillis - recordMillis) / 1000);
                    doc["ts"] = (uint32_t) backfilled;
                    changedAny = true;
                }
            }
            serializeJson(doc, dst);
            dst.println();
        } else {
            dst.println(line);   // couldn't parse — preserve the line as-is rather than dropping it
        }
    }
    src.close();
    if (!changedAny) {
        dst.close();
        SPIFFS.remove(tmpPath);
        return;
    }
    dst.close();
    SPIFFS.remove(logPath);
    SPIFFS.rename(tmpPath, logPath);
}

// Drop the oldest ~25% of records (by byte count) so a following append of
// `incoming` bytes fits under MAX_LOG_BYTES. Streams line-by-line so it never
// holds more than one record in RAM, regardless of file size.
static void trimIfNeeded(size_t incoming) {
    File probe = SPIFFS.open(logPath, "r");
    if (!probe)
        return;
    size_t size = probe.size();
    probe.close();
    if (size + incoming <= MAX_LOG_BYTES)
        return;

    File src = SPIFFS.open(logPath, "r");
    File dst = SPIFFS.open(tmpPath, "w");
    if (!src || !dst)
        return;

    size_t skipTarget = size / 4;
    size_t seen = 0;
    while (src.available()) {
        String line = src.readStringUntil('\n');
        seen += line.length() + 1;
        if (seen <= skipTarget)
            continue;
        dst.println(line);
    }
    src.close();
    dst.close();
    SPIFFS.remove(logPath);
    SPIFFS.rename(tmpPath, logPath);
}

static void appendRecord(JsonDocument &doc) {
    trimIfNeeded(measureJson(doc) + 2);
    File f = SPIFFS.open(logPath, "a");
    if (!f)
        return;
    serializeJson(doc, f);
    f.println();
    f.close();
}

static CharStat *findOrAddChar(char c) {
    for (uint8_t i = 0; i < segment.numChars; i++)
        if (segment.chars[i].c == c)
            return &segment.chars[i];
    if (segment.numChars >= MAX_LESSON_CHARS)
        return nullptr;   // defensive: shouldn't happen, charset is bounded
    CharStat *s = &segment.chars[segment.numChars++];
    s->c = c;
    s->heard = 0;
    s->attempts = 0;
    s->errors = 0;
    return s;
}

void beginSegment(uint8_t lesson, const char* mode) {
    endSegment();   // flush any previously open segment first
    if (!enabled())
        return;
    segment.active = true;
    segment.lesson = lesson;
    strncpy(segment.mode, mode, sizeof(segment.mode) - 1);
    segment.mode[sizeof(segment.mode) - 1] = '\0';
    segment.startMillis = millis();
    segment.numChars = 0;
    segment.words = 0;
    segment.totalChars = 0;
}

void wordPresented(const String &text) {
    if (!segment.active)
        return;
    if (segment.words < 0xFFFF)
        segment.words++;
    segment.totalChars += text.length();
    for (size_t i = 0; i < text.length(); i++) {
        CharStat *s = findOrAddChar(text.charAt(i));
        if (s && s->heard < 0xFFFF)
            s->heard++;
    }
}

void endSegment() {
    if (!segment.active)
        return;
    unsigned long durMs = millis() - segment.startMillis;
    segment.active = false;

    // Skip near-empty segments (e.g. entering and immediately leaving a Koch
    // menu without practicing) so navigation churn doesn't spam the log.
    if (durMs < 1000 && segment.numChars == 0)
        return;

    unsigned long flushMillis = millis();
    time_t ts = 0;
    if (hasWallClock()) {
        time_t now;
        time(&now);
        ts = now - (time_t)(durMs / 1000);
    }

    // Listen segments now populate one chars[] entry per distinct character
    // *heard* (not just sent/evaluated), so numChars can reach the full Koch
    // charset (51) — size generously enough for that worst case.
    DynamicJsonDocument doc(400 + segment.numChars * 56);
    doc["ts"]     = (uint32_t) ts;
    doc["m"]      = (uint32_t) flushMillis;   // millis() at flush — lets backfillTimestamps() date this later if ts is still 0
    doc["dur"]    = (uint32_t) (durMs / 1000);
    doc["lesson"] = segment.lesson;
    doc["mode"]   = segment.mode;
    doc["words"]  = segment.words;
    doc["tc"]     = segment.totalChars;   // total characters presented (listen + send)
    // Read live rather than snapshotting at beginSegment(): WPM in particular is commonly
    // adjusted via the encoder right after starting a session, and a start-of-segment
    // snapshot would log the pre-adjustment value for the whole session (only the *next*
    // session would show the change). Reading fresh at flush time reflects the speed the
    // session actually ended at, which is what the user just set.
    doc["ics"]    = MorsePreferences::pliste[posInterCharSpace].value;
    doc["iws"]    = MorsePreferences::pliste[posInterWordSpace].value;
    doc["wpm"]    = MorsePreferences::wpm;
    if (segment.numChars) {
        JsonObject chars = doc.createNestedObject("chars");
        for (uint8_t i = 0; i < segment.numChars; i++) {
            JsonArray a = chars.createNestedArray(String(segment.chars[i].c));
            a.add(segment.chars[i].heard);
            a.add(segment.chars[i].attempts);
            a.add(segment.chars[i].errors);
        }
    }
    appendRecord(doc);
}

void recordWord(const String &expected, const String &received) {
    if (!segment.active)
        return;
    for (size_t i = 0; i < expected.length(); i++) {
        CharStat *s = findOrAddChar(expected.charAt(i));
        if (!s)
            continue;
        s->attempts++;
        if (i >= received.length() || received.charAt(i) != expected.charAt(i))
            s->errors++;
    }
}

size_t usedBytes()  { return SPIFFS.usedBytes(); }
size_t totalBytes() { return SPIFFS.totalBytes(); }

void clearLog() {
    segment.active = false;
    if (SPIFFS.exists(logPath))
        SPIFFS.remove(logPath);
}

void begin() {
    // Nothing to do: SPIFFS is already mounted by setup(), and enabled() /
    // the log file are read/created lazily on first use.
}

}  // namespace MorsePracticeStats

#endif  // CONFIG_PRACTICE_STATS
