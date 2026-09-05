// Test-local stand-in for the toolchain's Arduino.h, found ahead of it via -I.
// ClickButton.h includes <Arduino.h>; everything it actually needs from it is in
// arduino_pin_shim.h, where the clock and the pin are scripted by the test.
#ifndef ARDUINO_H_TEST_STUB_
#define ARDUINO_H_TEST_STUB_
#include "arduino_pin_shim.h"
#endif
