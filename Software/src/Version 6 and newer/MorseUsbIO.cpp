// USB keyboard (vBand) + USB MIDI output for the M32 Pocket. See MorseUsbIO.h.
#include "MorseUsbIO.h"
#include "MorsePreferences.h"         // posUsbOut: the USB Output selector

#ifdef CONFIG_USB_HOSTIO

#if ARDUINO_USB_MODE
#error "CONFIG_USB_HOSTIO needs ARDUINO_USB_MODE=0 (TinyUSB on the USB-OTG controller)"
#endif

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "esp32-hal-tinyusb.h"
#include "tusb.h"

// ---------------------------------------------------------------- keyboard
// A global object: its constructor registers the HID interface before the USB stack starts.
static USBHIDKeyboard usbKeyboard;
static const uint8_t HID_LEFT_CTRL = 0xE0;          // HID usage: Keyboard Left Control (vBand)

// ---------------------------------------------------------------- MIDI
// The Arduino core (2.0.x) has no MIDI class, but TinyUSB has MIDI compiled in and the core lets a
// sketch add its own interface: supply the descriptor, then talk to tud_midi_*.
static const uint8_t MIDI_CHANNEL   = 0;            // 0 = MIDI channel 1
static const uint8_t NOTE_DIT       = 20;
static const uint8_t NOTE_DAH       = 21;
static const uint8_t NOTE_KEYED     = 30;           // straight key = CTR2-MIDI Extended paddle mode
static const uint8_t NOTE_VELOCITY  = 127;

static uint16_t midiDescriptor(uint8_t *dst, uint8_t *itf) {
    uint8_t strIndex = tinyusb_add_string_descriptor("Morserino MIDI");
    uint8_t epOut = tinyusb_get_free_out_endpoint();
    uint8_t epIn  = tinyusb_get_free_in_endpoint();
    uint8_t desc[] = { TUD_MIDI_DESCRIPTOR(*itf, strIndex, epOut, (uint8_t)(0x80 | epIn), 64) };
    *itf += 2;                                      // MIDI uses two interfaces (audio control + streaming)
    memcpy(dst, desc, sizeof(desc));
    return sizeof(desc);
}

// Registered from a static initialiser, like the core's own classes, so it is in place before
// USB.begin() builds the configuration descriptor.
static bool midiRegistered = []() {
    // The core starts USB in app_main(), before setup() - so the name has to be set here too.
    USB.manufacturerName("Morserino-32");
    USB.productName("Morserino-32 Pocket (USB kbd+MIDI)");
    return tinyusb_enable_interface(USB_INTERFACE_MIDI, TUD_MIDI_DESC_LEN, midiDescriptor) == ESP_OK;
}();

static void midiNote(uint8_t note, bool on) {
    if (!midiRegistered || !tud_midi_mounted())
        return;
    uint8_t msg[3] = { (uint8_t)((on ? 0x90 : 0x80) | MIDI_CHANNEL), note, (uint8_t)(on ? NOTE_VELOCITY : 0) };
    tud_midi_stream_write(0, msg, sizeof(msg));
}

// ---------------------------------------------------------------- state
static bool ctrlDown = false, keyedDown = false, ditDown = false, dahDown = false;
static bool handDit = false, handDah = false;       // the operator's paddles (after Paddle Polarity)
static bool tapDit = false, tapDah = false;         // a keyer memory "tapping" a paddle, see element()
static unsigned long tapUntil = 0;

static uint8_t usbOut() { return MorsePreferences::pliste[posUsbOut].value; }
static bool straightKey() { return MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY; }
static void setCtrl(bool down);

void MorseUsbIO::begin() {
    usbKeyboard.begin();
    USB.begin();                                    // a no-op: CDC-on-boot already started it in app_main()
}

// What goes out always looks like a human operator: on MIDI, paddle modes send only the paddle notes
// 20/21 and Straight Key mode only note 30 (the CTR2-MIDI's straight-key note) - a keyer memory included,
// which taps the paddles (element()) or works the straight key just as a person would. Never both at once,
// or the app would key the rig twice. Releases always go out (a key-up must never be lost).
void MorseUsbIO::keyed(bool down, bool generated) {
    (void) generated;
    setCtrl(down && usbOut() == USB_OUT_VBAND);
    bool note = down && usbOut() == USB_OUT_MIDI && straightKey();
    if (note == keyedDown)
        return;
    keyedDown = note;
    midiNote(NOTE_KEYED, note);
}

static void updatePaddleNotes() {
    bool midi = usbOut() == USB_OUT_MIDI && !straightKey();
    bool dit = (handDit || tapDit) && midi;
    bool dah = (handDah || tapDah) && midi;
    if (dit != ditDown) { ditDown = dit; midiNote(NOTE_DIT, dit); }
    if (dah != dahDown) { dahDown = dah; midiNote(NOTE_DAH, dah); }
}

void MorseUsbIO::paddles(bool dit, bool dah) {
    handDit = dit;
    handDah = dah;
    service();
}

// A keyer memory is about to sound one element. In the paddle modes, press the matching paddle note for
// half the element and let go - as an operator taps a paddle and lifts off before the app's keyer would
// repeat the element. The app's keyer forms the element, so its speed must match the Morserino's.
void MorseUsbIO::element(bool dah, unsigned long elementMs) {
    tapDit = !dah;
    tapDah = dah;
    tapUntil = millis() + elementMs / 2;
    updatePaddleNotes();
}

void MorseUsbIO::service() {
    if ((tapDit || tapDah) && (long) (millis() - tapUntil) >= 0)
        tapDit = tapDah = false;
    updatePaddleNotes();
}

static void setCtrl(bool down) {
    if (down == ctrlDown)
        return;
    ctrlDown = down;
    if (down) usbKeyboard.pressRaw(HID_LEFT_CTRL);
    else      usbKeyboard.releaseRaw(HID_LEFT_CTRL);
}

void MorseUsbIO::tick(bool inKeyer) {
    if (!inKeyer) {                                 // left CW Keyer (or never in it): nothing may stay held
        setCtrl(false);
        keyed(false);
        tapDit = tapDah = false;
        paddles(false, false);
    }
}


#else   // not CONFIG_USB_HOSTIO: everything compiles away

void MorseUsbIO::begin() {}
void MorseUsbIO::keyed(bool, bool) {}
void MorseUsbIO::paddles(bool, bool) {}
void MorseUsbIO::element(bool, unsigned long) {}
void MorseUsbIO::service() {}
void MorseUsbIO::tick(bool) {}

#endif
