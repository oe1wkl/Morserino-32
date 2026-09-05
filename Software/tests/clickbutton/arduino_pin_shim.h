// arduino_pin_shim.h — just enough of Arduino.h to compile and drive the real
// ClickButton.cpp on the host. The button and the clock are scripted by the
// test: sim_now() feeds millis(), and sim_pin() feeds digitalRead(), so a whole
// press/release waveform can be replayed deterministically, with the polling
// pattern (how often Update() is called, and when the caller stops looking)
// under the test's control too. That polling pattern is the whole point: the
// faults this guards against are not about the waveform at all, they are about
// a caller reading `clicks` on its first Update() after somebody else has
// already consumed the same press.

#ifndef ARDUINO_PIN_SHIM_H_
#define ARDUINO_PIN_SHIM_H_

#include <cstdint>

// ARDUINO itself is defined by the Makefile (-DARDUINO=200), so that ClickButton.h
// takes its <Arduino.h> path in every translation unit, this one included.

typedef bool boolean;
typedef uint8_t byte;

#define HIGH 1
#define LOW  0
#define INPUT        0
#define INPUT_PULLUP 2

extern unsigned long sim_millis_value;   // driven by the test
extern int           sim_pin_value;      // 0 = closed/pressed (active-low), 1 = open

inline unsigned long millis()               { return sim_millis_value; }
inline int  digitalRead(uint8_t)            { return sim_pin_value; }
inline void pinMode(uint8_t, uint8_t)       { }
inline void digitalWrite(uint8_t, uint8_t)  { }

#endif /* ARDUINO_PIN_SHIM_H_ */
