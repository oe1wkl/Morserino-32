/******************************************************************************************************************************
 *  M32KIP Phase 0 spike — see M32KipSpike.h. TEMPORARY; delete when Phase 2 lands.
 *****************************************************************************************************************************/
#ifdef KIP_SPIKE

#include "morsedefs.h"
#include "MorseOutput.h"
#include "MorseWiFi.h"
#include "MorsePreferences.h"
#include "M32KipSpike.h"
#include "esp_timer.h"

extern boolean checkPaddles();

namespace {

const uint16_t SPIKE_PORT      = 7374;      // the M32KIP port (spec §5)
const uint32_t TX_INTERVAL_US  = 20000;     // one outgoing packet every 20 ms, like KEY packets at ~25 pps
const uint32_t DRAW_INTERVAL_US = 250000;   // display at 4 Hz, the ceiling the spec allows the Rig (§7.6)
const uint32_t MIN_ALARM_US    = 5000;      // one-shot alarms 5..60 ms apart, the range of real edge spacings
const uint32_t ALARM_SPAN_US   = 55000;

// ---- shared with the ISR ----
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t* tmr = nullptr;
volatile uint64_t target = 0;               // alarm value of the pending one-shot, timer ticks (1 µs)
volatile uint32_t isrN = 0, isrMax = 0, isrOver200 = 0, isrOver1000 = 0, isrCore = 9;
volatile uint64_t isrSum = 0;
volatile int32_t  isrNeg = 0;               // alarms that fired "early" (should never happen; a sign of a wrapped compare)
uint32_t lcg = 12345;
uint8_t level = 0;

void onTimer() {                            // ISR context (level interrupt, non-IRAM: deferred during flash writes)
    uint64_t now = timerRead(tmr);
    int32_t lat = (int32_t)(now - target);
    level ^= 1;
    digitalWrite(keyerPin, level);          // the real key line, same call keyTransmitter() uses
    portENTER_CRITICAL_ISR(&mux);
    isrN++;
    if (lat < 0) { isrNeg++; lat = 0; }
    isrSum += lat;
    if ((uint32_t)lat > isrMax) isrMax = lat;
    if (lat > 200)  isrOver200++;
    if (lat > 1000) isrOver1000++;
    isrCore = xPortGetCoreID();
    portEXIT_CRITICAL_ISR(&mux);
    lcg = lcg * 1664525u + 1013904223u;
    target = now + MIN_ALARM_US + ((lcg >> 8) % ALARM_SPAN_US);
    timerAlarmWrite(tmr, target, false);
    timerAlarmEnable(tmr);
}

// ---- loop-side stats ----
uint32_t loopN = 0, loopMax = 0, loopOver2ms = 0, drawMax = 0;
// The loop cycles through three load states, 60 s each, and keeps the longest no-redraw gap per state:
//   0 = everything on (touch reads + UDP send, what an interactive mode does)
//   1 = touch reads off (a Remote Keyer mode with a straight key on the jack needs no touch sensing)
//   2 = touch reads and UDP send off (what remains: WiFi RX callbacks, buttons, timekeeping)
const uint32_t STATE_INTERVAL_US = 60000000;
uint8_t loadState = 0;
uint32_t loopMaxNoDraw[3] = {0, 0, 0};
uint64_t loopSum = 0;
uint32_t rxPackets = 0, txPackets = 0;
IPAddress peer;
uint16_t peerPort = 0;
bool havePeer = false;

void resetStats() {
    portENTER_CRITICAL(&mux);
    isrN = isrMax = isrOver200 = isrOver1000 = 0; isrSum = 0; isrNeg = 0;
    portEXIT_CRITICAL(&mux);
    loopN = loopMax = loopOver2ms = drawMax = 0; loopSum = 0;
    loopMaxNoDraw[0] = loopMaxNoDraw[1] = loopMaxNoDraw[2] = 0;
    rxPackets = txPackets = 0;
}

String pad(uint32_t v, uint8_t w) {         // right-aligned decimal, capped at w digits
    String s(v);
    if (s.length() > w) s = String("9999999999").substring(0, w);   // saturate rather than overflow the column
    while (s.length() < w) s = " " + s;
    return s;
}

void draw() {
    uint32_t n, mx, o200, o1000, core; uint64_t sum; int32_t neg;
    portENTER_CRITICAL(&mux);
    n = isrN; mx = isrMax; o200 = isrOver200; o1000 = isrOver1000; sum = isrSum; core = isrCore; neg = isrNeg;
    portEXIT_CRITICAL(&mux);
    uint32_t avg = n ? (uint32_t)(sum / n) : 0;
    // Line budget: 14 chars (OLED, NoOfCharsPerLine). All times in µs, fields saturate.
    //   ISR <max>/<avg><s>   one-shot alarm latency (Rig emitter error); <s> = current load state
    //                        (' ' all on, 'x' touch off, 'X' touch+UDP off), '!' = an alarm fired early (spike bug)
    //   D<max> >2<n>         longest display redraw; loop() gaps longer than 2 ms (all states)
    //   <a>x<b>X<c>          longest no-redraw loop() gap per load state, 4 digits each (9999 = saturated)
    static const char stateTag[3] = {' ', 'x', 'X'};
    MorseOutput::printOnScroll(0, REGULAR, 0, "ISR" + pad(mx, 5) + "/" + pad(avg, 4) + (neg ? "!" : String(stateTag[loadState])));
    MorseOutput::printOnScroll(1, REGULAR, 0, "D" + pad(drawMax, 5) + " >2" + pad(loopOver2ms, 4));
    MorseOutput::printOnScroll(2, REGULAR, 0, pad(loopMaxNoDraw[0], 4) + "x" + pad(loopMaxNoDraw[1], 4) + "X" + pad(loopMaxNoDraw[2], 4));
    if (NoOfVisibleLines > 3)
        MorseOutput::printOnScroll(3, REGULAR, 0, "rx" + pad(rxPackets, 6) + " tx" + pad(txPackets, 6) + " c" + String(core));
}

} // namespace

void M32KipSpike::run() {
    MorseOutput::clearDisplay();
    MorseOutput::printOnStatusLine(true, 0, "KIP spike");
    // No credentials, or the network is not there: step aside and let the normal firmware boot, so
    // WiFi can be configured on the device (rebooting here locked the device into a loop - seen on
    // the first classic flash).
    if (MorsePreferences::wlanSSID == "") {
        MorseOutput::printOnScroll(0, REGULAR, 0, "No WiFi conf -");
        MorseOutput::printOnScroll(1, REGULAR, 0, "normal boot");
        delay(2500);
        return;
    }
    MorseOutput::printOnScroll(0, REGULAR, 0, "Connecting...");
    if (!MorseWiFi::wifiConnect()) {           // shows its own "Not connected" screen for 3.5 s
        MorseOutput::clearDisplay();
        MorseOutput::printOnScroll(0, REGULAR, 0, "Spike skipped -");
        MorseOutput::printOnScroll(1, REGULAR, 0, "normal boot");
        delay(2000);
        return;
    }
    // Modem power save is on by default in the Arduino core and puts 50-500 ms into the RTT tail
    // (measured on the classic, PHASE0_RESULTS.md). The KIP modes will run with it off; measure likewise.
    WiFi.setSleep(false);
    MorseOutput::clearDisplay();
    MorseOutput::printOnStatusLine(true, 0, "KIP " + WiFi.localIP().toString());

    MorseWiFi::audp.listen(SPIKE_PORT);
    MorseWiFi::audp.onPacket([](AsyncUDPPacket packet) {   // runs in the async-UDP task on core 0
        rxPackets++;
        peer = packet.remoteIP(); peerPort = packet.remotePort(); havePeer = true;
        packet.write(packet.data(), packet.length());      // echo, so the host can measure RTT and loss
    });

    pinMode(keyerPin, OUTPUT);
    digitalWrite(keyerPin, LOW);
    tmr = timerBegin(0, 80, true);                          // group 0 / timer 0, 1 MHz, allocated on this core (1)
    timerAttachInterrupt(tmr, onTimer, false);
    target = timerRead(tmr) + 10000;
    timerAlarmWrite(tmr, target, false);
    timerAlarmEnable(tmr);

    uint8_t txBuf[40];
    memset(txBuf, 0x4B, sizeof(txBuf));
    uint64_t lastLoop = esp_timer_get_time(), lastTx = lastLoop, lastDraw = lastLoop;
    bool drewLastPass = false;
    uint64_t lastState = lastLoop;
    for (;;) {
        uint64_t now = esp_timer_get_time();
        uint32_t gap = (uint32_t)(now - lastLoop);
        lastLoop = now;
        loopN++; loopSum += gap;
        if (gap > loopMax) loopMax = gap;
        if (gap > 2000) loopOver2ms++;
        if (!drewLastPass && gap > loopMaxNoDraw[loadState]) loopMaxNoDraw[loadState] = gap;
        drewLastPass = false;

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == -1) {             // long press: back to a normal boot
            timerAlarmDisable(tmr);
            digitalWrite(keyerPin, LOW);
            ESP.restart();
        }
        if (Buttons::modeButton.clicks == 1) {
            resetStats();
            Buttons::modeButton.clicks = 0;
        }

        if ((uint32_t)(now - lastState) >= STATE_INTERVAL_US) {
            lastState = now;
            loadState = (loadState + 1) % 3;
        }
        if (loadState == 0)
            checkPaddles();                                 // the touch reads every interactive mode pays for

        if (loadState < 2 && havePeer && (uint32_t)(now - lastTx) >= TX_INTERVAL_US) {
            lastTx = now;
            MorseWiFi::audp.writeTo(txBuf, sizeof(txBuf), peer, peerPort);
            txPackets++;
        }
        if ((uint32_t)(now - lastDraw) >= DRAW_INTERVAL_US) {
            lastDraw = now;
            draw();
            uint32_t took = (uint32_t)(esp_timer_get_time() - now);
            if (took > drawMax) drawMax = took;
            drewLastPass = true;
        }
    }
}
#endif // KIP_SPIKE
