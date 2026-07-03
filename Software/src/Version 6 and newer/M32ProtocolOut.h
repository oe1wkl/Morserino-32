#ifndef M32PROTOCOLOUT_H_
#define M32PROTOCOLOUT_H_

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

#include <Arduino.h>

/// Output sink for the M32 serial protocol, so protocol traffic can reach more
/// than one transport (USB-CDC serial and, with CONFIG_BLE_SERIAL, BLE).
///
/// Emission vs delivery: code that *generates* protocol output gates on
/// protocolActive() (any transport handshaken); m32out then *delivers* per
/// transport — USB iff m32protocol, BLE iff bleProtocol and the link is up.
/// A USB port that never sent "PUT device/protocol/on" stays byte-silent even
/// while a BLE session is active.
///
/// DEBUG() (m32_v6.ino) and the RadioLib boot prints deliberately stay on raw
/// Serial: they are unframed text that would corrupt a BLE client's JSON
/// stream, and they fire pre-handshake anyway. Do not "fix" them into the tee.

extern boolean m32protocol;               // USB handshake state (m32_v6.ino)
#ifdef CONFIG_BLE_SERIAL
extern volatile bool bleProtocol;         // BLE handshake state (m32_v6.ino)
inline bool protocolActive() { return m32protocol || bleProtocol; }
#else
inline bool protocolActive() { return m32protocol; }
#endif

class M32Tee : public Print {
  public:
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    void echo(const String& s);           // raw keyed/generated character stream (SerialOutMorse):
                                          // USB always (caller gates on posSerialOut), BLE iff handshaken
    using Print::write;
};

extern M32Tee m32out;

#endif /* #ifndef M32PROTOCOLOUT_H_ */
