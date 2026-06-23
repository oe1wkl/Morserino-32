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
#endif

void MorseVoice::announce(const String& text) {
#ifdef CONFIG_AUDIO_A11Y
    String s = text;
    s.trim();                          // display strings are space-padded; decoder adds a trailing space
    if (s.length() == 0)
        return;
    const char *key = s.c_str();

    // voiceLookup[] is sorted by strcmp() byte order -> binary search.
    int lo = 0, hi = (int)voiceLookupCount - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        int cmp = strcmp(key, voiceLookup[mid].key);
        if (cmp == 0) {
            char path[24];             // "/voice/" + 8 hex + ".mp3" + NUL = 20
            snprintf(path, sizeof(path), "/voice/%s.mp3", voiceLookup[mid].id);
            MorseOutput::playVoiceClip(path);
            return;
        }
        if (cmp < 0) hi = mid - 1;
        else         lo = mid + 1;
    }
    // No clip for this string (e.g. an out-of-range numeric value): stay silent.
#else
    (void)text;
#endif
}
