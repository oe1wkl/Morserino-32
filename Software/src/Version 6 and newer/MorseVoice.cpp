/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2025  Willi Kraml, OE1WKL                                                                            ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *****************************************************************************************************************************/

#include "MorseVoice.h"

#ifdef CONFIG_AUDIO_A11Y
#include "MorseOutput.h"
#include "MorsePreferences.h"   // sidetoneVolume for the alarm; pulls in SPIFFS via morsedefs.h
#include "voice_clips.h"   // generated: voiceLookup[] (UI string -> clip id), sorted by strcmp
#include <string.h>
#ifdef CONFIG_AUDIO_A11Y_DIAG
#include <esp_heap_caps.h>
#endif

// An "utterance" is a short sequence of clip ids (e.g. heading + value, or "pro sign" + two
// phonetic letters). announce() starts a new pending utterance; announceMore() appends to it.
// Once navigation settles, tick() plays the pending utterance clip-by-clip (no mid-clip
// interrupt -- tearing the decoder down mid-play races the audio task). Latest pending wins.
// 12 slots: the longest utterance is the boot splash ("Morserino 32 accessibility edition,
// version 9 point 0 beta, battery 4 point 1 volts" = 11 clips). Anything past MV_MAX is
// silently dropped, so keep a slot or two of headroom.
#define MV_MAX 12
static char     seq[MV_MAX][9];            // active utterance (ids being played)
static int      seqLen = 0, seqPos = 0;
static char     pend[MV_MAX][9];           // pending utterance (awaiting its settle window)
static int      pendLen = 0;
static uint32_t pendAt = 0;
static bool     playing = false;
static const uint32_t DEBOUNCE_MS = 120;

static const char* lookupId(const char *key) {     // binary search voiceLookup[] (strcmp-sorted)
    int lo = 0, hi = (int)voiceLookupCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = strcmp(key, voiceLookup[mid].key);
        if (cmp == 0) return voiceLookup[mid].id;
        if (cmp < 0) hi = mid - 1; else lo = mid + 1;
    }
    return nullptr;
}

static const char* idFor(const String& text) {
    String s = text; s.trim();             // display strings are space-padded; decoder adds a trailing space
    return s.length() ? lookupId(s.c_str()) : nullptr;
}

static void appendId(const char *id) {     // push one clip onto the pending utterance
    if (id && pendLen < MV_MAX) {
        strncpy(pend[pendLen], id, 8); pend[pendLen][8] = '\0'; pendLen++;
        pendAt = millis();
    }
}

// ---- missing clip store: detection and alarm ---------------------------------------------
// When /voice/ is empty, announce() and friends are silent no-ops: startClip() fails its
// SPIFFS.exists() test, tick() moves on, and the device simply says nothing. CW still works
// perfectly, so what the operator gets is a machine that looks dead. That is the worst
// possible failure for the one build whose entire audience cannot see the screen.
//
// It is not hypothetical. The clips live in SPIFFS, not in the firmware image, so they are
// lost whenever the image is flashed but `-t uploadfs` is not (chaining `-t upload
// -t uploadfs` in one command tends to leave the filesystem unwritten - this is an ESP32-S3
// on native USB and the port re-enumerates after `upload`), and whenever SPIFFS fails to
// mount and setup()'s SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED) reformats a half-written image.

// Why the store is unusable. The order is the order the probe runs its checks in; extend it
// rather than replace it when the pack-version stamp arrives (see probeClipStore()).
enum ClipVerdict : int8_t {
    CV_UNKNOWN = -1,        // not probed yet
    CV_OK      =  0,
    CV_MISSING =  1,        // /voice/ empty or gutted: never uploaded, or SPIFFS reformatted
    CV_STALE   =  2,        // clips present, but from a pack this firmware was not built for
};
static int8_t clipVerdict = CV_UNKNOWN;

// The alarm: four high-low pairs on the sidetone, then silence. Deliberately not speech and
// not Morse. The clips are the thing that is missing, so they cannot carry their own bad
// news, and Morse would assume both CW literacy and a speed the operator can copy - neither
// is safe for a build whose users include complete beginners. A wide, repeated two-tone
// warble is heard as "something is wrong" by anyone, and its meaning is written down in the
// user manual (section on the Accessibility Edition).
//
// 1200 Hz sits above the whole sidetone range (MorseOutput::notes[] tops out at 932 Hz), so
// the alarm cannot be mistaken for CW at any Pitch setting; 600 Hz is an octave below it and
// still reproduces well on the Pocket's small speaker. hz == 0 is a silent step.
static const struct { uint16_t hz; uint16_t ms; } WARN_TONES[] = {
    {1200, 180}, {600, 180}, {0, 220},
    {1200, 180}, {600, 180}, {0, 220},
    {1200, 180}, {600, 180}, {0, 220},
    {1200, 180}, {600, 180},                 // ~2.1 s in total, which fits inside the boot
};                                           // splash's existing pauses: boot gets no longer
static const uint8_t WARN_TONE_STEPS = sizeof(WARN_TONES) / sizeof(WARN_TONES[0]);

static uint8_t  warnStep   = 0;
static uint32_t warnNextAt = 0;
static bool     warnActive = false;

static void warnTonesOff() {
    MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
    warnActive = false;
}

// Ordered checks, cheapest and most fundamental first: is there a pack at all, and is it
// this firmware's pack.
static int8_t probeClipStore() {
    // Probe a spread of known ids with the very test startClip() performs, rather than
    // listing /voice/: SPIFFS is a flat store whose directory support is emulated, and
    // exists() is the thing that actually decides whether a clip will play.
    //
    // A handful of probes cannot prove the store is COMPLETE, and that is deliberate: one
    // clip that failed to render must not nag on every boot for the rest of the device's
    // life. We report only the store gutted - none, or almost none, of the sample present -
    // which is what a missing or reformatted filesystem looks like. Spreading the sample
    // across the table also catches an upload that was cut off part-way.
    const uint8_t probes = 8;
    uint8_t found = 0;
    char path[24];                                 // "/voice/" + 8 hex + ".mp3" + NUL
    for (uint8_t i = 0; i < probes; i++) {
        unsigned int idx = (unsigned int) ((uint32_t) i * voiceLookupCount / probes);
        snprintf(path, sizeof(path), "/voice/%s.mp3", voiceLookup[idx].id);
        if (SPIFFS.exists(path)) found++;
    }
    if (found * 2 < probes) {                      // under half present = gutted
        Serial.println("Voice clip store missing or empty (" + String(found) + "/" +
                       String(probes) + " probes found) - upload it with: "
                       "pio run -e pocketwroom-accessibility -t uploadfs");
        return CV_MISSING;
    }

    // There ARE clips. Are they this firmware's clips? Clip names are content hashes, so a
    // firmware whose strings changed looks for ids an older pack never contained: the device
    // stays silent on exactly those entries and nothing says why. /voice/pack.txt names the
    // set (written by generate_audio.sh); VOICE_PACK_STAMP is what this build expects.
    File pf = SPIFFS.open("/voice/pack.txt", FILE_READ);
    if (!pf)
        return CV_OK;   // no stamp: a pack from before stamping, or one the generator refused
                        // to vouch for. We cannot tell right from wrong, so we do not cry
                        // wolf - same rule as an absent NVS version stamp (CLAUDE.md section 4).
    char stamp[16];
    size_t n = pf.readBytes(stamp, sizeof(stamp) - 1);
    pf.close();
    stamp[n] = '\0';
    for (size_t i = 0; i < n; i++)                 // cut at the trailing newline / any junk
        if (stamp[i] < '0' || stamp[i] > 'z') { stamp[i] = '\0'; break; }
    if (strcmp(stamp, VOICE_PACK_STAMP) != 0) {
        Serial.println("Voice pack is for another firmware (pack " + String(stamp) +
                       ", this build wants " + String(VOICE_PACK_STAMP) + ") - refresh it "
                       "with: pio run -e pocketwroom-accessibility -t uploadfs");
        return CV_STALE;
    }
    return CV_OK;
}
#endif

void MorseVoice::announce(const String& text) {        // start a NEW pending utterance
#ifdef CONFIG_AUDIO_A11Y
    const char *id = idFor(text);
    pendLen = 0;                                        // replace whatever was pending
    if (id) { strncpy(pend[pendLen], id, 8); pend[pendLen][8] = '\0'; pendLen = 1; }
    pendAt = millis();
#else
    (void)text;
#endif
}

void MorseVoice::announceMore(const String& text) {    // append to the pending utterance
#ifdef CONFIG_AUDIO_A11Y
    appendId(idFor(text));
#else
    (void)text;
#endif
}

void MorseVoice::announceMoreChar(const String& ch) {  // append one character, spelled out
#ifdef CONFIG_AUDIO_A11Y
    // Keyed by the RAW firmware character (before cleanUpProSigns): the uppercase prosign
    // codes expand to "pro sign" + two phonetics, so one character can be three clips.
    // Silent for a character with no entry -- better than mispronouncing it.
    for (unsigned int i = 0; i < voiceCharLookupCount; i++) {
        if (ch == voiceCharLookup[i].key) {
            for (unsigned char k = 0; k < voiceCharLookup[i].n; k++)
                appendId(voiceCharLookup[i].ids[k]);
            return;
        }
    }
#else
    (void)ch;
#endif
}

void MorseVoice::tick() {
#ifdef CONFIG_AUDIO_A11Y
    if (warnActive) {                                  // the alarm owns the sidetone
        if ((int32_t)(millis() - warnNextAt) >= 0) {
            if (warnStep >= WARN_TONE_STEPS) {
                warnTonesOff();
                Serial.println("Voice alarm: finished");
            } else {
                const uint16_t hz = WARN_TONES[warnStep].hz;
                if (hz) MorseOutput::pwmTone(hz, MorsePreferences::sidetoneVolume, false);
                else    MorseOutput::pwmNoTone(MorsePreferences::sidetoneVolume);
                warnNextAt = millis() + WARN_TONES[warnStep].ms;
                warnStep++;
            }
        }
        return;                                        // there are no clips to play anyway
    }
    if (playing) {                                     // advance the clip in progress
        if (!MorseOutput::voiceService()) {
            playing = false; seqPos++;
#ifdef CONFIG_AUDIO_A11Y_DIAG
            // Leak diagnosis for the (former) very-late freeze: watch these across hundreds
            // of clips -- a steady decline means something still accumulates per clip.
            Serial.printf("[a11y] clip done: heap=%u minEver=%u maxBlock=%u\n",
                          ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                          heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
        }
        else return;
    }
    char path[24];                                     // "/voice/" + 8 hex + ".mp3" + NUL
    if (seqPos < seqLen) {                             // continue the active utterance (next clip)
        snprintf(path, sizeof(path), "/voice/%s.mp3", seq[seqPos]);
        MorseOutput::voiceStart(path); playing = true;
        return;
    }
    if (pendLen > 0 && (millis() - pendAt) >= DEBOUNCE_MS) {   // active done -> start settled pending
        for (int i = 0; i < pendLen; i++) memcpy(seq[i], pend[i], 9);
        seqLen = pendLen; seqPos = 0; pendLen = 0;
        snprintf(path, sizeof(path), "/voice/%s.mp3", seq[0]);
        MorseOutput::voiceStart(path); playing = true;
    }
    // NB: the decoder is reset per clip by the AUDIO TASK itself (vendor/cw-i2s-sidetone,
    // audioLoop/teardownClip) -- that is the only context where end()/begin() cannot race
    // the copier. Resetting from here (the UI task) crashed every time it was tried.
#endif
}

void MorseVoice::stop() {
#ifdef CONFIG_AUDIO_A11Y
    if (warnActive)                     // "any input silences the announcement" applies to the
        warnTonesOff();                 // alarm exactly as it does to a spoken one
    pendLen = 0; seqLen = 0; seqPos = 0;
    MorseOutput::voiceStop();
    playing = false;
#endif
}

bool MorseVoice::clipStoreOk() {
#ifdef CONFIG_AUDIO_A11Y
    if (clipVerdict == CV_UNKNOWN)
        clipVerdict = probeClipStore();
    return clipVerdict == CV_OK;
#else
    return true;                        // no clips wanted, so nothing can be missing
#endif
}

bool MorseVoice::clipStoreUsable() {
#ifdef CONFIG_AUDIO_A11Y
    clipStoreOk();                      // force the probe, then read the cached verdict
    return clipVerdict != CV_MISSING;   // a mismatched pack still plays most of what it holds
#else
    return true;
#endif
}

const char* MorseVoice::clipStoreDisplayText() {
#ifdef CONFIG_AUDIO_A11Y
    if (clipStoreOk())
        return nullptr;
    // Bounded by NoOfCharsPerLine (24 on the TFT). These messages are deliberately NOT
    // voiced, and must stay that way: a clip for them would be missing in exactly the
    // situation they exist to report. That is CLAUDE.md section 8's documented exception,
    // not a gap somebody still has to fill. The fix itself goes to the serial log (see
    // probeClipStore()) and to the user manual, which also explains the alarm tones.
    return (clipVerdict == CV_STALE) ? "Wrong voice pack!" : "No voice clips!";
#else
    return nullptr;
#endif
}

void MorseVoice::warnClipStore() {
#ifdef CONFIG_AUDIO_A11Y
    if (clipStoreOk())
        return;
    warnStep   = 0;
    warnNextAt = millis();              // first step fires on the very next tick()
    warnActive = true;
    // Only ever printed when something is already wrong, like the probe's own line above.
    // It separates "never got here" from "sounded it and you heard nothing", which are
    // very different bugs: the first is detection, the second is the audio path.
    Serial.println("Voice alarm: " + String(WARN_TONE_STEPS) + " tone steps, volume " +
                   String(MorsePreferences::sidetoneVolume));
#endif
}
