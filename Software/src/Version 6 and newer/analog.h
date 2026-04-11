#ifndef _ANALOGREADFAST_H_
#define _ANALOGREADFAST_H_
#ifdef __cplusplus
extern "C" {
#endif

/* Driver to read ADC1 really fast and/or do asynchronous (non-blocking) conversions.
 * Caution: Functions are not reentrant - be careful with threads and interrupts.
 *          Apply a wrapper with locks/mutexes if necessary.
 * 
 * fadcInit(<number-of-pins>, <pin>(, <pin>(, ...))
 * Initializes ADC and pins - don't use regular analogRead and friends after initializing!
 * 
 * analogReadFast(<channel>)
 * Read the specified channel, which should be in the list of pins during initialization
 * 
 * analogReadMilliVoltsFast(<channel>)
 * Same as analogReadFast, but returns calibrated millivolt reading.
 * 
 * fadcStart(<channel>)
 * Start a conversion and return immediately - don't call unless you know the ADC is not busy
 * 
 * fadcBusy()
 * Returns true if the ADC is still converting
 * 
 * fadcResult()
 * Return the result of the conversion - don't call unless you know a conversion has completed
 * 
 * fadcApply(<value>)
 * Apply calibration and conversion to millivolts
 * Takes a value in the range 0-2**FADC_CAL_RESOLUTION (typically 0-4095)
 */

#include <stdint.h>
#include <stdbool.h>
#include <hal/adc_hal.h>
#include <esp32-hal-adc.h>

#define FADC_CAL_USE                 // Comment out to remove everything calibration-related
#define FADC_RESOLUTION           12 // Set ADC to 12-bit ADC resolution
#define FADC_ATTEN          ADC_11db // Set ADC to 11dB attenuation

#ifdef FADC_CAL_USE
#define FADC_CAL_SIZE              6 // Calibration table with (1**N)=64 points (RAM use: 2 bytes per point)
#define FADC_CAL_RESOLUTION       12 // Conversion takes input in range 0-4095 (1**N-1)

// Number of bits to shift left by to make fadcResult suitable for fadcApply
#define FADC_SHIFT (FADC_CAL_RESOLUTION - FADC_RESOLUTION)
#endif

// Library functions

void fadcInit(uint8_t pins, ...);

#ifdef FADC_CAL_USE
uint16_t fadcApply(uint32_t v);
#endif

static inline void  __attribute__((always_inline)) fadcStart(uint8_t channel) {
    SENS.sar_meas1_ctrl2.sar1_en_pad = (1 << channel);
    SENS.sar_meas1_ctrl2.meas1_start_sar = 0;
    SENS.sar_meas1_ctrl2.meas1_start_sar = 1;
}

static inline bool __attribute__((always_inline)) fadcBusy() {
    return !(bool)SENS.sar_meas1_ctrl2.meas1_done_sar;
}

static inline uint16_t  __attribute__((always_inline)) fadcResult() {
    return HAL_FORCE_READ_U32_REG_FIELD(SENS.sar_meas1_ctrl2, meas1_data_sar);
}

// Arduino-style "easy" functions

static inline uint16_t  __attribute__((always_inline)) analogReadFast(uint8_t channel) {
    fadcStart(channel);
    while(fadcBusy());
    return fadcResult();
}

#ifdef FADC_CAL_USE
static inline uint16_t __attribute__((always_inline)) analogReadMilliVoltsFast(uint8_t channel)  {
    return fadcApply(analogReadFast(channel) << FADC_SHIFT);
}
#endif

#ifdef __cplusplus
}
#endif
#endif
