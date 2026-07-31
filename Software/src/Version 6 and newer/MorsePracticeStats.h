#ifndef MORSEPRACTICESTATS_H_
#define MORSEPRACTICESTATS_H_

/******************************************************************************************************************************
 *  Practice Stats — Koch-lesson time and per-character error-rate logging (M32 Pocket)
 *  Copyright (C) 2026  Willi Kraml, OE1WKL
 *
 *  Part of the Morserino-32 firmware. See main license.
 *
 *  Records one JSON-lines entry per contiguous practice "segment" (a span of
 *  time in a Koch-active mode at a fixed lesson) to /stats.jsonl on SPIFFS:
 *  duration, lesson, and per-character attempt/error counts. Viewed via the
 *  WiFi "Practice Stats" web page (MorseWiFi::viewStats()) — see
 *  devdocs/practice-stats/README.md for the log format and design rationale.
 *****************************************************************************************************************************/

#include <Arduino.h>
#include <time.h>

#ifdef CONFIG_PRACTICE_STATS

namespace MorsePracticeStats {

    extern const char* const logPath;

    void begin();                          // call once from setup(), after SPIFFS.begin()

    bool enabled();                        // NVS-backed on/off (default: on)
    void setEnabled(bool on);

    // Segment lifecycle: call beginSegment() when entering a Koch-active mode
    // (or when the lesson changes mid-practice), endSegment() when leaving one
    // or before the device sleeps. Safe to call endSegment() with no segment
    // open (no-op). Starting a new segment while one is already open implicitly
    // closes the previous one first.
    void beginSegment(uint8_t lesson, const char* mode);
    void endSegment();

    // Full character-by-character diff of expected vs. received, accumulated
    // into the currently open segment. No-op if no segment is open.
    void recordWord(const String &expected, const String &received);

    // Count a new word/group being presented (generator playback as well as
    // echo trainer prompts — call regardless of mode; a "send" segment's word
    // count comes from this same call, not from recordWord()). Also credits
    // each character in `text` with one "heard" — the only per-character signal
    // "listen" segments ever get, since they never call recordWord(). No-op if
    // no segment is open.
    void wordPresented(const String &text);

    // Wall-clock sync (web UI or serial). epochSeconds must be a real Unix
    // timestamp; 0 is treated as "unknown" and never accepted.
    void setWallClock(time_t epochSeconds);
    bool hasWallClock();

    // Best-effort NTP sync: no-op if hasWallClock() is already true. Otherwise
    // starts SNTP and blocks up to ~4s waiting for a sane time. Call only once
    // WiFi is actually connected (station mode with a route to the internet —
    // silently does nothing useful on a LAN with no internet access, which is
    // fine, since the browser-based /api/time sync still covers that case).
    void tryNtpSync();

    size_t usedBytes();
    size_t totalBytes();
    void   clearLog();

}  // namespace MorsePracticeStats

#endif  // CONFIG_PRACTICE_STATS
#endif  // MORSEPRACTICESTATS_H_
