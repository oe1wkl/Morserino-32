// USB keyboard (vBand) + USB MIDI output for the M32 Pocket. See MorseUsbIO.h.
#include "MorseUsbIO.h"
#include "MorsePreferences.h"         // posUsbOut: the USB Keying selector

#ifdef CONFIG_USB_HOSTIO

#if !ARDUINO_USB_MODE
#error "CONFIG_USB_HOSTIO needs ARDUINO_USB_MODE=1: boot on HWCDC, TinyUSB started at run time (see MorseUsbIO.h)"
#endif

#include "USB.h"
#include "USBHIDKeyboard.h"
#include "esp32-hal-tinyusb.h"
#include "tusb.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/usb_serial_jtag_reg.h"
#include "driver/periph_ctrl.h"
#include "MorseJSON.h"
#include "M32ProtocolOut.h"

extern uint32_t rebootMagic;                        // m32_v6.ino: survives the restart
extern boolean checkKey();                          // m32_v6.ino: a paddle or key is down

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
static const uint8_t NOTE_VAIL_KEY  = 0;            // straight key on Vail's web tools, which ignore note 30
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
// USB.begin() builds the configuration descriptor. (The names the computer shows come from the
// USB_MANUFACTURER / USB_PRODUCT build flags. Setting them from a static initialiser is unreliable: the
// order of static construction across files is unspecified, so the core's USB object may be built after
// it and put its defaults back.)
static bool midiRegistered =
    tinyusb_enable_interface(USB_INTERFACE_MIDI, TUD_MIDI_DESC_LEN, midiDescriptor) == ESP_OK;

static bool otgStarted = false;                     // TinyUSB owns the USB port until the next restart

static void midiNote(uint8_t note, bool on) {
    if (!otgStarted || !midiRegistered || !tud_midi_mounted())
        return;
    uint8_t msg[3] = { (uint8_t)((on ? 0x90 : 0x80) | MIDI_CHANNEL), note, (uint8_t)(on ? NOTE_VELOCITY : 0) };
    tud_midi_stream_write(0, msg, sizeof(msg));
}

// ---------------------------------------------------------------- state
static bool ctrlDown = false, keyedDown = false, vailDown = false, ditDown = false, dahDown = false;
static bool handDit = false, handDah = false;       // the operator's paddles (after Paddle Polarity)
static bool tapDit = false, tapDah = false;         // a keyer memory "tapping" a paddle, see element()
static unsigned long tapUntil = 0;

static uint8_t usbOut() { return MorsePreferences::pliste[posUsbOut].value; }
static bool straightKey() { return MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY; }
static void setCtrl(bool down);

void MorseUsbIO::begin() {
    // USB Keying was active before this boot: a restart (or waking from deep sleep) leaves the PHY with the
    // OTG controller and HWCDC would never appear. periph_module_disable() alone would wrap the module's
    // 8-bit reference count from 0 to 255, and the later USB.begin() would then find the clock off and crash.
    if (REG_GET_BIT(RTC_CNTL_USB_CONF_REG, RTC_CNTL_SW_HW_USB_PHY_SEL)) {
        periph_module_enable(PERIPH_USB_MODULE);
        periph_module_reset(PERIPH_USB_MODULE);
        periph_module_disable(PERIPH_USB_MODULE);
        CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG, RTC_CNTL_SW_HW_USB_PHY_SEL | RTC_CNTL_SW_USB_PHY_SEL | RTC_CNTL_USB_PAD_ENABLE);
        CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PHY_SEL);
    }
    usbKeyboard.begin();                            // no USB.begin() here: HWCDC keeps the port until tick()
}

// MIDI Paddles looks like a human operator: paddle modes send only the paddle notes 20/21 and Straight Key
// mode only note 30 (the CTR2-MIDI's straight-key note) - a keyer memory included, which taps the paddles
// (element()) or works the straight key just as a person would. Never both at once, or the app would key the
// rig twice. MIDI Keyer sends no paddle notes at all: notes 0 and 30 follow whatever CW Keyer keys. Releases
// always go out, to the notes that were pressed (a key-up must never be lost).
void MorseUsbIO::keyed(bool down, bool generated) {
    (void) generated;
    uint8_t out = usbOut();
    setCtrl(down && out == USB_OUT_VBAND);
    bool note = down && (out == USB_OUT_MIDI_KEYER || (out == USB_OUT_MIDI && straightKey()));
    if (note == keyedDown)
        return;
    keyedDown = note;
    if (note)
        vailDown = out == USB_OUT_MIDI_KEYER;
    midiNote(NOTE_KEYED, note);
    if (vailDown)
        midiNote(NOTE_VAIL_KEY, note);
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
    if (!otgStarted || down == ctrlDown)
        return;
    ctrlDown = down;
    if (down) usbKeyboard.pressRaw(HID_LEFT_CTRL);
    else      usbKeyboard.releaseRaw(HID_LEFT_CTRL);
}

// The port is about to leave HWCDC. A protocol session cannot survive that, and left open it would block
// the time-out for good (checkShutDown), so say goodbye while the host can still hear it.
static void startOtg() {
    otgStarted = true;
    if (m32protocol) {
        M32TargetScope usbOnly(M32Target::UsbOnly);
        MorseJSON::jsonCreate("end m32protocol", "Goodbye!", "");
        m32out.flushReply();
        delay(50);
        m32protocol = false;
    }
    USB.begin();
}

void MorseUsbIO::tick(bool inKeyer) {
    if (inKeyer && !otgStarted && usbOut() != USB_OUT_OFF)
        startOtg();
    if (otgStarted && tud_midi_mounted()) {         // Vail writes control changes to every MIDI output: discard them
        uint8_t packet[4];
        while (tud_midi_available())
            tud_midi_packet_read(packet);
    }
    if (!inKeyer) {                                 // left CW Keyer (or never in it): nothing may stay held
        setCtrl(false);
        keyed(false);
        tapDit = tapDah = false;
        paddles(false, false);
    }
}

// Back at the menu with USB started: restart, which is what gives the port back to HWCDC (see begin()).
// A finger still on the encoder (the long press that got us here) or on a paddle would read as a gesture
// at boot, or be calibrated into the touch sensors, so wait for it - but not for ever.
void MorseUsbIO::menuEntered() {
    if (!otgStarted)
        return;
    MorsePreferences::writeVolume();                // the menu would have done it
    tick(false);                                    // the computer must not be left with a key held down
    delay(20);
    unsigned long giveUp = millis() + 2000;
    do {
        Buttons::modeButton.Update();
        delay(10);
    } while ((Buttons::modeButton.depressed || checkKey()) && (long) (millis() - giveUp) < 0);
    rebootMagic = USB_BACK_REBOOT_MAGIC;
    ESP.restart();
}


#else   // not CONFIG_USB_HOSTIO: everything compiles away

void MorseUsbIO::begin() {}
void MorseUsbIO::keyed(bool, bool) {}
void MorseUsbIO::paddles(bool, bool) {}
void MorseUsbIO::element(bool, unsigned long) {}
void MorseUsbIO::service() {}
void MorseUsbIO::tick(bool) {}
void MorseUsbIO::menuEntered() {}

#endif
