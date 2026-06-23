#ifndef MORSEVOICE_H_
#define MORSEVOICE_H_

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

#include <Arduino.h>

// V9.0 audio accessibility (M32 Pocket "Accessibility" build, CONFIG_AUDIO_A11Y).
// Speaks menu / preference / option entries as pre-rendered MP3 clips played on-device.
// announce() is a no-op on builds without CONFIG_AUDIO_A11Y, so call sites stay clean.
namespace MorseVoice
{
  // Request announcement of a firmware-facing UI string (menu display text, spoken pref
  // label, option value, numeric value...). Non-blocking and debounced: the latest request
  // wins, and tick() starts playback once navigation settles. Silent if the string has no
  // clip (e.g. an out-of-range number). No-op without CONFIG_AUDIO_A11Y.
  void announce(const String& text);
  // Drive async playback (advance/finish current clip, fire the settled request). Must be
  // polled frequently from the menu / preference loops.
  void tick();
  // Interrupt + clear any current/pending announcement (e.g. when leaving the menu).
  void stop();
}

#endif /* #ifndef MORSEVOICE_H_ */
