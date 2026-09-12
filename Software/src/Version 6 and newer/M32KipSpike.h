#ifndef M32KIPSPIKE_H_
#define M32KIPSPIKE_H_
/******************************************************************************************************************************
 *  M32KIP Phase 0 spike — TEMPORARY diagnostic, not part of the shipped feature.
 *
 *  Answers the two questions in devdocs/m32kip/IMPLEMENTATION_PLAN.md Phase 0 without a logic analyser:
 *    1. How late does a hardware-timer one-shot ISR on core 1 fire under WiFi load (Rig-side emitter, spec §7.6)?
 *    2. How coarse is the loop() cadence under the same load (Keyer-side polled edge capture, decision D2)?
 *  Both are self-measured against the hardware timer / esp_timer and shown on the display, refreshed 4x per second.
 *
 *  Build with the flag on the command line, never in platformio.ini:
 *      PLATFORMIO_BUILD_FLAGS="-D KIP_SPIKE=1" pio run -e pocketwroom -t upload
 *  The device then boots straight into the spike (setup() hands over, never returns). Long-press the encoder
 *  to reboot, single-click to reset the counters. WiFi load: Software/tests/kip/spike_load.py <ip>.
 *  The TX key line toggles on every ISR - disconnect any transmitter.
 *****************************************************************************************************************************/
#ifdef KIP_SPIKE
namespace M32KipSpike {
    void run();            // never returns
}
#endif
#endif /* M32KIPSPIKE_H_ */
