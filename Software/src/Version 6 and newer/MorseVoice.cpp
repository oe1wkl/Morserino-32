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
#include "voice_clips.h"   // generated: voiceLookup[] (UI string -> clip id), sorted by strcmp
#include <string.h>

// An "utterance" is a short sequence of clip ids (e.g. heading + value, or "pro sign" + two
// phonetic letters). announce() starts a new pending utterance; announceMore() appends to it.
// Once navigation settles, tick() plays the pending utterance clip-by-clip (no mid-clip
// interrupt -- tearing the decoder down mid-play races the audio task). Latest pending wins.
#define MV_MAX 8
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
    const char *id = idFor(text);
    if (id && pendLen < MV_MAX) {
        strncpy(pend[pendLen], id, 8); pend[pendLen][8] = '\0'; pendLen++;
        pendAt = millis();
    }
#else
    (void)text;
#endif
}

void MorseVoice::tick() {
#ifdef CONFIG_AUDIO_A11Y
    if (playing) {                                     // advance the clip in progress
        if (!MorseOutput::voiceService()) { playing = false; seqPos++; }
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
    // NB: an idle-pause decoder reset (end()/begin()) was tried here to clear the slow
    // accumulation behind the very-late freeze, but end()/begin() crashes even when idle, so
    // it is removed. The slow leak needs a different approach (see HANDOFF / heap diagnosis).
#endif
}

void MorseVoice::stop() {
#ifdef CONFIG_AUDIO_A11Y
    pendLen = 0; seqLen = 0; seqPos = 0;
    MorseOutput::voiceStop();
    playing = false;
#endif
}
