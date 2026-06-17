#ifndef MORSEMIDI_H_
#define MORSEMIDI_H_

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

#ifdef CONFIG_BLE_MIDI

namespace MorseMidi {
    bool isRunning();
    extern bool isMidiConnected;
    void initializeMidi();
    void stopMidi();
    void noteOn(uint8_t note);
    void noteOff(uint8_t note);
}

#endif // CONFIG_BLE_MIDI
#endif // MORSEMIDI_H_
