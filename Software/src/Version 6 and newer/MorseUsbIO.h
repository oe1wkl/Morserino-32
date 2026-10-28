#ifndef MORSEUSBIO_H_
#define MORSEUSBIO_H_
/// USB keyboard (vBand) + USB MIDI output for the M32 Pocket - CONFIG_USB_HOSTIO builds only.
///
/// The stock Pocket runs its USB port as the ESP32-S3's fixed hardware USB-Serial-JTAG (HWCDC),
/// which can only ever be a serial port. CONFIG_USB_HOSTIO builds (env pocketwroom-usbio) switch to
/// the chip's USB-OTG controller with TinyUSB (ARDUINO_USB_MODE=0) so one cable carries a composite
/// device: the usual serial port (config tool, file manager, M32 protocol, WinKeyer) PLUS
/// a USB keyboard and a USB MIDI device. The "USB Output" preference (CW Keyer) picks what is sent:
///   * VBand: Left Ctrl is held down while the keyer keys - what the Bluetooth vBand keyboard sends,
///     minus the BLE latency (the Bluetooth keyboard is not started meanwhile);
///   * MIDI (channel 1): note 20 = dit paddle, note 21 = dah paddle (after the Paddle Polarity setting -
///     the Lynovation CTR2-MIDI Normal mode / TinyMIDI convention);
///   * Off: nothing but the serial port (and charging).
/// Recovery if a build ever misbehaves: hold FN while plugging in USB (ROM download mode), then run
/// the official installer. Added by VK1HF, 2026-10-28 - not part of the upstream firmware.

#include <Arduino.h>

namespace MorseUsbIO {
    void begin();                       // call once in setup(), before Serial.begin()
    void keyed(bool down);              // keyer output edge (keyTransmitter / key off)
    void paddles(bool dit, bool dah);   // debounced paddle state, polarity already applied
    void tick(bool inKeyer);            // every loop pass: releases anything held when not in CW Keyer
}

#endif
