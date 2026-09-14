/******************************************************************************************************************************
 *  M32KIP Rig unit. See MorseKipRig.h, devdocs/m32kip/SPEC.md §7-§9, and devdocs/m32kip/PHASE0_RESULTS.md.
 *****************************************************************************************************************************/

#include "MorseKipRig.h"
#ifdef CONFIG_M32KIP

#include "M32Kip.h"
#include "MorseOutput.h"
#include "MorseWiFi.h"
#include "MorsePreferences.h"
#include "MorseMenu.h"
#include "MorseJSON.h"
#include "M32ProtocolOut.h"
#include <esp_system.h>

using namespace M32Kip;

extern void    serialEvent();
extern boolean goToMenu;

namespace {

// ---------------------------------------------------------------- shared with the ISR
//
// The ISR is deliberately tiny: it writes one pre-computed level to the key line and records that it fired. All of the
// deciding happens in the run loop, which arms the alarm for the next edge. Keeping the queue out of interrupt context
// is what lets the reconstruction stay ordinary C++ with no locking around it.

hw_timer_t*       gTimer = nullptr;
volatile uint8_t  gPendingLevel = KEY_UP;
volatile bool     gFired = false;
volatile uint32_t gFiredAtUs = 0;

void IRAM_ATTR onKipTimer() {
    digitalWrite(keyerPin, gPendingLevel ? HIGH : LOW);
    gFiredAtUs = (uint32_t)timerRead(gTimer);
    gFired = true;
}

// ---------------------------------------------------------------- receive ring
//
// AsyncUDP delivers on the WiFi core; the reconstruction runs here. The callback does nothing but copy the datagram, so
// the two never contend for the session state.

struct RxPkt {
    uint8_t   data[MAX_PACKET];
    uint16_t  len;
    IPAddress from;
    uint16_t  port;
};

const uint8_t RX_RING = 12;
RxPkt             gRx[RX_RING];
volatile uint8_t  gRxHead = 0, gRxTail = 0;
portMUX_TYPE      gRxMux = portMUX_INITIALIZER_UNLOCKED;

void onUdp(AsyncUDPPacket packet) {
    if (packet.length() > MAX_PACKET) return;
    portENTER_CRITICAL(&gRxMux);
    uint8_t next = (uint8_t)((gRxHead + 1) % RX_RING);
    if (next != gRxTail) {                          // a full ring drops the newest: the stream is self-repairing
        gRx[gRxHead].len = (uint16_t)packet.length();
        memcpy(gRx[gRxHead].data, packet.data(), packet.length());
        gRx[gRxHead].from = packet.remoteIP();
        gRx[gRxHead].port = packet.remotePort();
        gRxHead = next;
    }
    portEXIT_CRITICAL(&gRxMux);
}

bool popRx(RxPkt& out) {
    bool got = false;
    portENTER_CRITICAL(&gRxMux);
    if (gRxTail != gRxHead) {
        out = gRx[gRxTail];
        gRxTail = (uint8_t)((gRxTail + 1) % RX_RING);
        got = true;
    }
    portEXIT_CRITICAL(&gRxMux);
    return got;
}

// ---------------------------------------------------------------- clock
//
// One clock for everything: the hardware timer, at 1 MHz. The rig clock the protocol wants is that divided by 100, and
// arming the alarm from the same counter means no conversion error between deciding and firing.

inline uint64_t nowUs()    { return timerRead(gTimer); }
inline uint32_t nowTicks() { return (uint32_t)(nowUs() / 100); }

void armFor(uint32_t tickTime, uint8_t level) {
    int32_t delta = tickDiff(tickTime, nowTicks());         // may be slightly negative: fire at once
    int64_t target = (int64_t)nowUs() + (int64_t)delta * 100;
    if (target <= (int64_t)nowUs() + 20) target = (int64_t)nowUs() + 20;
    gPendingLevel = level;
    gFired = false;
    timerAlarmWrite(gTimer, (uint64_t)target, false);
    timerAlarmEnable(gTimer);
}

void keyLineUp() {
    timerAlarmDisable(gTimer);
    digitalWrite(keyerPin, LOW);
    gPendingLevel = KEY_UP;
}

// ---------------------------------------------------------------- session

uint8_t   gBaseKey[32], gSessKey[32];
uint32_t  gSession = 0;
bool      gHaveSession = false;
String    gLastEnd;                // why the last session ended, shown until the next one starts (D14)
IPAddress gPeer;
uint16_t  gPeerPort = 0;
uint16_t  gTxSeq = 0;

void sendTo(const uint8_t* buf, size_t len) {
    if (len) MorseWiFi::audp.writeTo(buf, len, gPeer, gPeerPort);
}

void sendBye() {
    if (!gHaveSession) return;
    uint8_t buf[MAX_PACKET];
    for (int i = 0; i < 3; i++) {                   // spec §6.6: three times, 100 ms apart
        Header h; h.session = gSession; h.seq = ++gTxSeq;
        sendTo(buf, encodeBye(buf, sizeof(buf), h, gSessKey));
        delay(100);
    }
}

/// Every way the Rig itself ends a session goes through here: key up first, then a BYE so the Keyer can
/// reconnect at once instead of waiting out its own timeout, then the reason for the display.
void endSession(const char* why) {
    keyLineUp();
    sendBye();
    gHaveSession = false;
    gLastEnd = why;
}

// ---------------------------------------------------------------- display, at most 4 Hz

void drawFrame(const RigSession& rig, const String& ip, bool listening, uint32_t sessions) {
    MorseOutput::clearScrollLines();
    if (listening) {
        MorseOutput::printOnScroll(0, REGULAR, 0, "Listening");
        MorseOutput::printOnScroll(1, REGULAR, 0, ip);
        MorseOutput::printOnScroll(2, REGULAR, 0, gLastEnd.length() ? gLastEnd : String("no session yet"));
    } else {
        const RigCounters& c = rig.counters();
        MorseOutput::printOnScroll(0, REGULAR, 0, String(rig.keyState() == KEY_DOWN ? "KEY " : "    ")
                                                  + "D" + String(ticksToMs(rig.playoutTicks())) + "ms");
        MorseOutput::printOnScroll(1, REGULAR, 0, "late " + String(c.late) + " und " + String(c.underruns));
        uint32_t dit = rig.speed().hasEstimate() ? rig.speed().ditEst() : 0;
        MorseOutput::printOnScroll(2, REGULAR, 0, dit ? ("~" + String(1200 / (ticksToMs(dit) ? ticksToMs(dit) : 1)) + " WpM")
                                                      : String("connected"));
    }
    MorseOutput::refreshDisplay();
}

} // namespace

// ================================================================ the mode

void MorseKipRig::run() {
    // --- the pre-shared key is what stands between a stranger and your transmitter (spec §9) ---
    if (MorsePreferences::kipPsk.length() < 12) {
        MorseOutput::clearDisplay();
        MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, "No key set");
        MorseOutput::printOnScroll(1, REGULAR, 0, "Set a pass-");
        MorseOutput::printOnScroll(2, REGULAR, 0, "phrase first");
        if (protocolActive())
            MorseJSON::jsonCreate("message", "M32KIP: no pre-shared key configured", "");
        delay(2500);
        return;
    }
    deriveBaseKey(MorsePreferences::kipPsk.c_str(), gBaseKey);

    MorseMenu::showStartDisplay("Remote Rig", "Unattended -", "keep on power", 1200);

    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, REGULAR, 0, "Connecting...");
    MorseOutput::refreshDisplay();
    if (!MorseWiFi::wifiConnect()) return;          // shows its own failure screen
    WiFi.setSleep(false);                           // modem sleep puts tens of ms into the arrival times

    String ip = WiFi.localIP().toString();
    MorseOutput::clearDisplay();
    MorseOutput::printOnStatusLine(true, 0, "Rig " + ip);

    pinMode(keyerPin, OUTPUT);
    digitalWrite(keyerPin, LOW);
    gTimer = timerBegin(0, 80, true);               // 1 MHz, allocated here so the ISR lands on this core
    timerAttachInterrupt(gTimer, onKipTimer, false);

    gRxHead = gRxTail = 0;
    gHaveSession = false;
    gLastEnd = "";
    gSession = 0;
    MorseWiFi::audp.listen(MorsePreferences::kipPort);
    MorseWiFi::audp.onPacket(onUdp);

    // Spec §11 defaults, with the D4 preferences on top. A chosen Playout Delay is the starting value AND the floor:
    // late edges may still raise it (the alternative is late edges on the air), and adaptation brings it back down
    // to the chosen value, never below. Plain `adaptive = false` would let every raise stick for the rest of the
    // session. Read once here: preferences cannot change while the mode runs.
    RigConfig cfg;
    uint16_t fixedD = MorsePreferences::kipPlayoutMs(MorsePreferences::pliste[posKipPlayout].value);
    if (fixedD) { cfg.dDefaultMs = fixedD; cfg.dMinMs = fixedD; }
    cfg.maxKeydownKeyerMs  = (uint16_t)(MorsePreferences::pliste[posKipMaxKeyer].value  * 1000u);
    cfg.maxKeydownManualMs = (uint16_t)(MorsePreferences::pliste[posKipMaxManual].value * 1000u);
    RigSession rig;
    uint32_t sessions = 0;
    uint32_t lastDraw = 0, lastStats = 0;
    bool armed = false;

    Buttons::modeButton.clicks = 0;
    Buttons::volButton.clicks = 0;

    for (;;) {
        uint32_t t = nowTicks();
        MorseOutput::resetTOT();                    // unattended by design: this mode never times out (D8)

        // ---- leaving ----
        // Servicing the protocol here is what makes `PUT menu/stop` work. Measured on 2026-09-13: without
        // it the mode swallowed every command, which is the wrong behaviour for a station meant to sit
        // unattended at a remote site - the one link to it would be the one thing that could not reach it.
        serialEvent();
        if (goToMenu) { goToMenu = false; break; }
        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == -1) break;             // long press leaves, as in every mode (D14)
        if (Buttons::modeButton.clicks == 1 && gHaveSession) {   // short press resets the session (D14); the
            endSession("End: reset");                           // Keyer reconnects on its own within a second
            armed = false;
            lastDraw = 0;
        }
        Buttons::modeButton.clicks = 0;

        // No paddle, touch-pad or key-jack override (D14). A Rig normally stands unattended, and in the first
        // two-device test a stray touch of the classic's pads could end a session with nobody the wiser.

        // ---- incoming ----
        RxPkt pkt;
        while (popRx(pkt)) {
            Header h;
            if (!peekHeader(pkt.data, pkt.len, h)) continue;

            if (h.type == PKT_HELLO) {
                Hello hello;
                if (!decodeHello(pkt.data, pkt.len, gBaseKey, h, hello)) continue;   // bad MAC: silence (D12c)
                if (gHaveSession && (pkt.from != gPeer)) {
                    Header nh; nh.session = 0; nh.seq = ++gTxSeq;
                    Nack nack; nack.reason = NACK_BUSY;
                    uint8_t buf[MAX_PACKET];
                    size_t n = encodeNack(buf, sizeof(buf), nh, nack, gBaseKey);
                    if (n) MorseWiFi::audp.writeTo(buf, n, pkt.from, pkt.port);
                    continue;                       // one session per Rig unit
                }
                HelloAck ack;
                memcpy(ack.nonceC, hello.nonceC, 8);
                for (int i = 0; i < 8; i++) ack.nonceS[i] = (uint8_t)(esp_random() & 0xFF);
                gSession = esp_random() | 1u;       // never zero
                ack.session = gSession;
                deriveSessionKey(gBaseKey, ack.nonceC, ack.nonceS, gSessKey);

                keyLineUp();                        // a replaced session must never leave a mark hanging
                gPeer = pkt.from; gPeerPort = pkt.port;
                rig.begin(nowTicks(), cfg, hello.source);
                ack.maxKeydownMs = rig.maxKeydownMs();
                ack.pttLeadMs = 0;                  // no isolated PTT output on either variant (D6)
                ack.flags = 0;
                gHaveSession = true;
                armed = false;
                sessions++;

                Header rh; rh.session = gSession; rh.seq = ++gTxSeq;
                uint8_t buf[MAX_PACKET];
                sendTo(buf, encodeHelloAck(buf, sizeof(buf), rh, ack, gBaseKey));
                lastDraw = 0;
                continue;
            }

            if (!gHaveSession || h.session != gSession) continue;

            if (h.type == PKT_BYE) {
                if (!decodeBye(pkt.data, pkt.len, gSessKey, h)) continue;
                keyLineUp();
                gHaveSession = false;
                gLastEnd = "End: BYE";
                armed = false;
                lastDraw = 0;
                continue;
            }
            if (h.type == PKT_KEY) {
                KeyPkt kp;
                if (!decodeKey(pkt.data, pkt.len, gSessKey, h, kp)) continue;
                rig.onKey(h, kp, nowTicks());
            }
        }

        // ---- reconstruction ----
        if (gHaveSession) {
            if (gFired) { gFired = false; armed = false; }

            RigAction act;
            while (rig.poll(nowTicks(), act)) {
                if (act.type == RIG_EMIT) {
                    // The ISR has already put this edge on the line; poll() is what retires it. Should the alarm
                    // somehow not have fired, write it now rather than leave the line wrong.
                    digitalWrite(keyerPin, act.state ? HIGH : LOW);
                    armed = false;
                } else if (act.type == RIG_FORCE_KEYUP) {
                    keyLineUp();
                    armed = false;
                } else if (act.type == RIG_DROP) {
                    RigDropReason why = rig.dropReason();
                    endSession(why == DROP_KEEPALIVE ? "End: timeout"  :
                               why == DROP_ERRORS    ? "End: errors"   :
                               why == DROP_OVERFLOW  ? "End: overflow" : "End: dropped");
                    armed = false;
                    lastDraw = 0;
                    break;
                }
            }
            if (gHaveSession && !armed && rig.hasPending()) {
                armFor(rig.nextEmitTime(), rig.nextEmitState());
                armed = true;
            }

            if (tickDiff(t, lastStats) >= (int32_t)msToTicks(1000)) {
                lastStats = t;
                Stats st;
                rig.fillStats(st, nowTicks());
                Header sh; sh.session = gSession; sh.seq = ++gTxSeq;
                uint8_t buf[MAX_PACKET];
                sendTo(buf, encodeStats(buf, sizeof(buf), sh, st, gSessKey));
            }
        }

        // ---- display, rate-limited: a redraw costs 37 ms on the OLED and 46 ms on the TFT ----
        if (tickDiff(t, lastDraw) >= (int32_t)msToTicks(250)) {
            lastDraw = t;
            drawFrame(rig, ip, !gHaveSession, sessions);
        }
    }

    // ---- leaving: the transmitter goes up first, then we say goodbye ----
    keyLineUp();
    sendBye();
    timerAlarmDisable(gTimer);
    timerDetachInterrupt(gTimer);
    timerEnd(gTimer);
    gTimer = nullptr;
    MorseWiFi::audp.close();
    gHaveSession = false;
    if (protocolActive())
        MorseJSON::jsonCreate("message", "Remote Rig ended", "");
}

#endif // CONFIG_M32KIP
