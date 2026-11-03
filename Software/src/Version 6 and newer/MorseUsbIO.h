#ifndef MORSEUSBIO_H_
#define MORSEUSBIO_H_
/// USB keyboard (vBand) + USB MIDI output for the M32 Pocket - CONFIG_USB_HOSTIO builds only.
///
/// The ESP32-S3 has two USB controllers on one PHY. The Pocket boots and normally runs on the fixed
/// hardware USB-Serial-JTAG (HWCDC), which can only ever be a serial port but which the silicon itself
/// can reset into download mode - so flashing never depends on the firmware. The USB-OTG controller with
/// TinyUSB can be a keyboard and a MIDI device. The image holds both, and the PHY changes hands at run time:
///   * CW Keyer with "USB Keying" not Off: tick() ends any USB protocol session and starts TinyUSB. The
///     computer now sees a USB keyboard + MIDI device, and no serial port;
///   * back at the menu: menuEntered() restarts the device (silently, see USB_BACK_REBOOT_MAGIC), because
///     boot is the one tested way back. begin() returns the PHY to HWCDC, also after waking from deep sleep,
///     which keeps the selection. So serial is off only while CW Keyer is sending to USB.
/// The "USB Keying" preference (CW Keyer) picks what is sent:
///   * VBand: Left Ctrl is held down while the keyer keys - what the Bluetooth vBand keyboard sends,
///     minus the BLE latency (the Bluetooth keyboard is not started meanwhile);
///   * MIDI Paddles (channel 1, the computer is the keyer): note 20 = dit paddle, note 21 = dah paddle (after
///     the Paddle Polarity setting - the Lynovation CTR2-MIDI Normal mode / TinyMIDI convention); in Straight
///     Key mode note 30 (its Extended mode). A keyer memory goes out the same way a person would send it:
///     paddle taps, or the key;
///   * MIDI Keyer (channel 1, the M32 is the keyer): notes 0 and 30 follow the finished keying, as VBand does.
///     Note 0 is the straight key for Vail's web tools (which ignore 30), note 30 for CTR2-style programs;
///   * Off: nothing but the serial port (and charging).

#include <Arduino.h>

#define USB_BACK_REBOOT_MAGIC 0xC0FFEE43u   // rebootMagic (m32_v6.ino): silent restart to the menu, no Quick Start

namespace MorseUsbIO {
    void begin();                       // call once in setup(), before Serial.begin()
    void keyed(bool down, bool generated = false);  // keyer output edge (keyTransmitter / key off);
                                                    // generated = a keyer memory is playing
    void paddles(bool dit, bool dah);   // debounced paddle state, polarity already applied
    void element(bool dah, unsigned long elementMs);  // a keyer memory starts a dit or dah: tap that paddle
    void service();                     // ends a memory's paddle tap on time (called while elements sound)
    void tick(bool inKeyer);            // every loop pass: starts USB in CW Keyer, releases anything held outside it
    void menuEntered();                 // arriving at the menu: restarts (never returns) if USB was started
}

#endif
