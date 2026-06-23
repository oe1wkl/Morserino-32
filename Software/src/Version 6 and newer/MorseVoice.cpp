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

static char     pendingId[12] = "";        // clip id awaiting its debounce window ("" = none)
static uint32_t pendingAt     = 0;
static bool     clipPlaying   = false;
static const uint32_t DEBOUNCE_MS = 120;   // let fast scrolling settle before we speak

// Binary search voiceLookup[] (sorted by strcmp byte order); nullptr if no clip for `key`.
static const char* lookupId(const char *key) {
    int lo = 0, hi = (int)voiceLookupCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = strcmp(key, voiceLookup[mid].key);
        if (cmp == 0) return voiceLookup[mid].id;
        if (cmp < 0) hi = mid - 1; else lo = mid + 1;
    }
    return nullptr;
}
#endif

void MorseVoice::announce(const String& text) {
#ifdef CONFIG_AUDIO_A11Y
    String s = text;
    s.trim();                              // display strings are space-padded; decoder adds a trailing space
    if (s.length() == 0)
        return;
    const char *id = lookupId(s.c_str());
    if (id == nullptr) {                   // no clip for this string: drop any stale pending, stay silent
        pendingId[0] = '\0';
        return;
    }
    strncpy(pendingId, id, sizeof(pendingId) - 1);
    pendingId[sizeof(pendingId) - 1] = '\0';
    pendingAt = millis();                  // (re)start the debounce window -> latest request wins
#else
    (void)text;
#endif
}

void MorseVoice::tick() {
#ifdef CONFIG_AUDIO_A11Y
    // advance / finish the clip that is currently playing
    if (clipPlaying && !MorseOutput::voiceService())
        clipPlaying = false;
    // Start the latest request only once any current clip has finished. We deliberately do NOT
    // interrupt a playing clip: tearing the decoder down mid-play races the audio task (a hang).
    // Each clip fully drains, so the result queue can't accumulate. Latest pending still wins, so
    // after a clip ends we speak wherever the user has landed -- with up to ~1 clip of lag.
    if (pendingId[0] && !clipPlaying && (millis() - pendingAt) >= DEBOUNCE_MS) {
        char path[24];                      // "/voice/" + 8 hex + ".mp3" + NUL
        snprintf(path, sizeof(path), "/voice/%s.mp3", pendingId);
        pendingId[0] = '\0';
        MorseOutput::voiceStart(path);
        clipPlaying = true;
    }
#endif
}

void MorseVoice::stop() {
#ifdef CONFIG_AUDIO_A11Y
    pendingId[0] = '\0';
    MorseOutput::voiceStop();
    clipPlaying = false;
#endif
}
