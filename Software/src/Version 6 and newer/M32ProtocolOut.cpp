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

#include "M32ProtocolOut.h"
#include "MorseWinKeyer.h"     // ownsSerial(): no M32 bytes on a port a WinKeyer host is reading
#ifdef CONFIG_BLE_SERIAL
#include "MorseBleSerial.h"     // confined to the .cpp — keeps MorseJSON.cpp free of a header cycle
#endif
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT   // Serial is HWCDC (not TinyUSB's USBCDC, not a UART)
#include "hal/usb_serial_jtag_ll.h"
#define M32_HWCDC_ZLP 1
#endif

M32Tee m32out;

size_t M32Tee::write(uint8_t c) {
	return write(&c, 1);
}

size_t M32Tee::write(const uint8_t *buffer, size_t size) {
#ifdef CONFIG_BLE_SERIAL
	if (target != M32Target::BleOnly && m32protocol
#ifdef CONFIG_WINKEYER
	    && !MorseWinKeyer::ownsSerial()
#endif
	   )
		Serial.write(buffer, size);
	if (target == M32Target::BleOnly)              // targeted: answers that client's own input, so
		MorseBleSerial::txEnqueue(buffer, size);   // deliver even pre-handshake (linkUp checked inside)
	else if (target == M32Target::All && bleProtocol)  // cheap gate only — txEnqueue does the full check
		MorseBleSerial::txEnqueue(buffer, size);
	return size;
#else
	// without BLE the tee degenerates to a plain forward: every call site is
	// only reachable under an m32protocol gate, so behavior is byte-identical
	// (M32Target::UsbOnly narrows nothing when USB is the only transport)
#ifdef CONFIG_WINKEYER
	if (MorseWinKeyer::ownsSerial())
		return size;
#endif
	return Serial.write(buffer, size);
#endif
}

// Push a finished command reply all the way out, instead of leaving it in the
// transport's transmit buffer.
//
// On the M32 Pocket, Serial is HWCDC — the ESP32-S3's hardware USB-Serial-JTAG.
// Its write() hands bytes to a ring buffer and an ISR drains them, but the
// interrupt that does the draining is only re-armed under `connected`; when it
// is not, the bytes simply sit there. The next thing to arrive on the wire
// wakes the ISR and they go out then — which is why an unanswered command was
// always answered the moment the NEXT command was sent. Arduino's own flush()
// exists for exactly this, and says so: "Now trigger the ISR to read data from
// the ring buffer."
//
// The symptom is worse than a slow reply: a client that gives up waiting reads
// the stale answer as the reply to its next command, and every reply after that
// is off by one. Software/tests/protocol caught this and proves it by counting
// objects — see the finding in its README.
//
// Deliberately NOT called after every protocol object. Only a client waiting on
// a command reply needs this; the asynchronous event stream and echo() are
// fire-and-forget, they get carried out by the next traffic anyway, and flushing
// per keyed character would put an up-to-100 ms wait (HWCDC's tx_timeout_ms)
// into the keying path if a host ever stopped reading. On a UART build this is
// a couple of milliseconds and harmless either way.
//
// flush() alone is not enough (found 2026-10-30, Software/tests/protocol/usb64_probe.py):
// HWCDC's ISR sends the ring in packets of up to 64 bytes and never follows a final
// full packet with a zero-length packet. To the host a full packet means "more to
// come", so a reply ending exactly on 64·k bytes stayed in its USB driver until the
// next traffic - every 64-byte reply did, and longer replies did when the ISR's
// chunking happened to end on a full packet (the "~1 in 900" of TODO D8).
// endUsbTransfer() sends the missing ZLP.
void M32Tee::flushReply() {
	Serial.flush();                        // BLE paces itself in MorseBleSerial; nothing to do there
#ifdef M32_HWCDC_ZLP
	endUsbTransfer();
#endif
}

#ifdef M32_HWCDC_ZLP
// Close the USB transfer after flush() has emptied the ring: wait for the host to
// collect the ISR's last packet from the endpoint FIFO, then flush the FIFO while it
// is empty - the USB-Serial/JTAG hardware sends that as a zero-length packet (ESP-IDF's
// own usb_serial_jtag driver ends every burst this way). Sent after every reply, not
// only after a full one: a ZLP after a short packet is an empty read the host drivers
// ignore, and HWCDC::write() itself flushes the empty FIFO on every call to wake its ISR.
// The wait is one USB frame (1 ms) normally, bounded so a host that stopped reading
// costs at most 10 ms. If another writer's packet slips in between the check and the
// flush, wr_done on a FIFO that is still waiting is a no-op (write() does the same).
void M32Tee::endUsbTransfer() {
	if (!HWCDC::isPlugged())
		return;
	uint32_t start = micros();
	while (!usb_serial_jtag_ll_txfifo_writable()) {   // last packet still waiting for the host
		if (micros() - start > 10000)
			return;
		delayMicroseconds(50);
	}
	usb_serial_jtag_ll_txfifo_flush();                // empty FIFO + wr_done = zero-length packet
}
#endif

void M32Tee::echo(const String& s) {
#ifdef CONFIG_WINKEYER
	if (!MorseWinKeyer::ownsSerial())   // the WinKeyer host would read the characters as echo bytes
#endif
	Serial.print(s);            // today's "Serial Output" behavior, handshake irrelevant
#ifdef CONFIG_BLE_SERIAL
	if (bleProtocol)            // cheap gate only — txEnqueueEcho does the full linkUp() check
		MorseBleSerial::txEnqueueEcho((const uint8_t *) s.c_str(), s.length());
#endif
}
