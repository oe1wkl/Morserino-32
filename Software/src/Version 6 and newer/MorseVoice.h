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
  // Speak the clip for a firmware-facing UI string (menu display text, spoken pref
  // label, option value, numeric value...). Trims the string first (the decoder/display
  // pad with spaces). Silent if the string has no clip (e.g. an out-of-range number).
  void announce(const String& text);
}

#endif /* #ifndef MORSEVOICE_H_ */
