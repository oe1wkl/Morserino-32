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

#ifdef KIP_MEASURE
// ---------------------------------------------------------------- Phase 5 instrument (devdocs/m32kip/TEST_REPORT.md)
//
// The logic analyser we do not have. `-D KIP_MEASURE=1` on the command line only, never in platformio.ini, like the
// Phase 0 spike. For every edge put on the line it compares the interval the ISR actually produced with the interval
// between the Keyer's own timestamps for the same two edges: a mark or a space reproduced exactly has error 0. By D12b
// that is the acceptance reference - the Keyer's key line, not the operator's hand.
//
// Spaces are graded in two classes, because the spec treats them differently: a space inside a character or between
// characters must not move; an idle gap (>= the estimator's idle threshold) is where the Rig may take up clock drift
// and shorten the playout delay, so a change there is legal and reported separately.

struct MeasHist {
    uint32_t n, b200, b500, b1000, b2000, over;     // |error| < 0.2 / 0.5 / 1 / 2 ms, and >= 2 ms
    int32_t  minUs, maxUs;
    void add(int32_t e) {
        uint32_t a = (uint32_t)(e < 0 ? -e : e);
        if (!n || e < minUs) minUs = e;
        if (!n || e > maxUs) maxUs = e;
        n++;
        if (a < 200) b200++; else if (a < 500) b500++; else if (a < 1000) b1000++; else if (a < 2000) b2000++; else over++;
    }
    String line(const char* name) const {
        if (!n) return String(name) + " -";
        return String(name) + " " + String(n) + " [" + String(b200) + "/" + String(b500) + "/" + String(b1000)
             + "/" + String(b2000) + "/" + String(over) + "] " + String(minUs) + ".." + String(maxUs) + "us";
    }
};

struct Meas {
    static const uint8_t TRACE = 48;
    bool     have;
    uint32_t prevT;         // sender ticks of the previous emitted edge
    uint32_t prevUs;        // when it went on the line, timer microseconds (modular: fine for intervals < 71 min)
    uint8_t  prevState;
    MeasHist marks, spaces, idle;
    uint32_t shortened;     // marks reproduced more than 0.2 ms short: design principle 5 says never
    // Per-interval trace of the first TRACE intervals of a session. Aggregates alone cannot tell a key line that
    // really moved from a measurement that paired the wrong two edges, and the first run showed errors of exactly
    // one dah and exactly one word gap - the signature of a mispairing, not of a timing fault.
    int32_t  trSent[TRACE], trGot[TRACE];
    uint8_t  trState[TRACE];
    char     trSrc[TRACE];  // where the line timestamp came from: I = this pass's ISR, P = carried over, N = clock read
    uint8_t  trN, trDumped;
    // The first TRACE intervals are where nothing goes wrong. Outliers turn up minutes into a run, so they get a
    // trace of their own: what a rare multi-millisecond slip needs is its SOURCE letter - an edge retired by the
    // loop's fallback write ('N') points at loop latency (the 37 ms OLED redraw), one timed by the alarm does not.
    static const uint8_t XTRACE = 16;
    int32_t  xErr[XTRACE];
    uint8_t  xState[XTRACE];
    char     xSrc[XTRACE];
    uint8_t  xN, xDumped;
    void reset() { *this = Meas(); }
    Meas() : have(false), prevT(0), prevUs(0), prevState(KEY_UP), marks(), spaces(), idle(), shortened(0),
             trN(0), trDumped(0), xN(0), xDumped(0) {}
};

Meas     gMeas;
uint32_t gLastFiredUs = 0;
bool     gFiredPending = false;

void measureEdge(const RigSession& rig, uint32_t senderT, uint8_t state, uint32_t lineUs, char src = '?') {
    if (gMeas.have && state != gMeas.prevState) {
        int32_t sentUs = tickDiff(senderT, gMeas.prevT) * 100;
        int32_t gotUs  = (int32_t)(lineUs - gMeas.prevUs);
        int32_t err    = gotUs - sentUs;
        if (gMeas.trN < Meas::TRACE) {
            gMeas.trSent[gMeas.trN] = sentUs;
            gMeas.trGot[gMeas.trN]  = gotUs;
            gMeas.trState[gMeas.trN] = state;
            gMeas.trSrc[gMeas.trN]  = src;
            gMeas.trN++;
        }
        if ((err > 500 || err < -500) && gMeas.xN < Meas::XTRACE) {
            gMeas.xErr[gMeas.xN]   = err;
            gMeas.xState[gMeas.xN] = state;
            gMeas.xSrc[gMeas.xN]   = src;
            gMeas.xN++;
        }
        if (state == KEY_UP) {                      // the interval that just ended was a mark
            gMeas.marks.add(err);
            if (err < -200) gMeas.shortened++;
        } else if ((uint32_t)sentUs / 100 >= rig.speed().idleGapTicks()) {
            gMeas.idle.add(err);
        } else {
            gMeas.spaces.add(err);
        }
    }
    gMeas.have = true;
    gMeas.prevT = senderT;
    gMeas.prevUs = lineUs;
    gMeas.prevState = state;
}

void measureReport(const RigSession& rig) {
    if (!protocolActive()) return;
    const RigCounters& c = rig.counters();
    MorseJSON::jsonCreate("message", "KIPM " + gMeas.marks.line("mark") + " | " + gMeas.spaces.line("space") + " | "
        + gMeas.idle.line("idle") + " | short " + String(gMeas.shortened) + " D " + String(ticksToMs(rig.playoutTicks()))
        + " late " + String(c.late) + " und " + String(c.underruns) + " dLow " + String(c.dLowers)
        + " off " + String(c.offSteps) + " dit " + String(ticksToMs(rig.speed().ditEst()))
        + " dup " + String(c.duplicates) + " perr " + String(c.protocolErrors)
        + " kdl " + String(c.keydownLimits) + " ovf " + String(c.overflows), "");

    // A few trace rows per report, so one report never sits long enough on the serial line to disturb the loop.
    if (gMeas.trDumped < gMeas.trN) {
        String s = "KIPT";
        uint8_t upto = (uint8_t)(gMeas.trDumped + 10);
        if (upto > gMeas.trN) upto = gMeas.trN;
        for (; gMeas.trDumped < upto; gMeas.trDumped++) {
            uint8_t i = gMeas.trDumped;
            s += " " + String(i) + (gMeas.trState[i] == KEY_UP ? "M" : "S")
               + String(gMeas.trSent[i]) + "/" + String(gMeas.trGot[i]) + String(gMeas.trSrc[i]);
        }
        MorseJSON::jsonCreate("message", s, "");
    }

    // The outliers, whenever any have been caught: how far off, whether the interval was a mark or a space, and
    // where the line timestamp came from. 'N' on a multi-millisecond slip means the loop retired the edge before
    // its alarm fired - loop latency, not the emitter.
    if (gMeas.xDumped < gMeas.xN) {
        String s = "KIPX";
        uint8_t upto = (uint8_t)(gMeas.xDumped + 6);
        if (upto > gMeas.xN) upto = gMeas.xN;
        for (; gMeas.xDumped < upto; gMeas.xDumped++) {
            uint8_t i = gMeas.xDumped;
            s += " " + String(gMeas.xErr[i]) + "us" + (gMeas.xState[i] == KEY_UP ? "M" : "S") + String(gMeas.xSrc[i]);
        }
        MorseJSON::jsonCreate("message", s, "");
    }
}
#endif

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
    // Break-in compensation (D16): the transceiver's hang is set like the transceiver itself - Yaesu-style in ms
    // (50 ms steps) or Icom-style in dits (half-dit steps).
    cfg.firstExtMs = MorsePreferences::pliste[posKipFirstExt].value;
    if (MorsePreferences::pliste[posKipHangUnit].value == 1)
        cfg.hangDitsX2 = MorsePreferences::pliste[posKipHang].value;
    else
        cfg.hangMs = (uint16_t)(MorsePreferences::pliste[posKipHang].value * 50u);
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
#ifdef KIP_MEASURE
                gMeas.reset();                      // one session, one measurement
                gFiredPending = false;
#endif
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
#ifdef KIP_MEASURE
            if (gFired) { gLastFiredUs = gFiredAtUs; gFiredPending = true; gFired = false; armed = false; }
#else
            if (gFired) { gFired = false; armed = false; }
#endif

            RigAction act;
            while (rig.poll(nowTicks(), act)) {
                if (act.type == RIG_EMIT) {
#ifdef KIP_MEASURE
                    // When did this edge reach the line? The ISR's own timestamp if the alarm fired for it - also if
                    // it fired only just now, after the check above - otherwise the write below. 'I', 'P' and 'N' say
                    // which, so a trace can be read without guessing.
                    uint32_t lineUs; char src;
                    if (gFired)             { lineUs = gFiredAtUs;   src = 'I'; gFired = false; }
                    else if (gFiredPending) { lineUs = gLastFiredUs; src = 'P'; }
                    else                    { lineUs = (uint32_t)nowUs(); src = 'N'; }
                    gFiredPending = false;
                    measureEdge(rig, act.senderT, act.state, lineUs, src);
#endif
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
#ifdef KIP_MEASURE
                // Arming a new alarm retires any fire still on the books. An edge emitted by the fallback write above
                // can have its alarm go off afterwards, and that timestamp belongs to an edge already on the line -
                // handing it to the NEXT edge made one interval read ~0 and the one after it a whole element long.
                gFiredPending = false;
#endif
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
#ifdef KIP_MEASURE
                // Every KIP_REPORT_S seconds; the counts are cumulative, so a longer interval loses nothing but
                // detail in time. It is a knob because the report itself writes ~200 bytes to the serial line from
                // THIS loop - about 17 ms at 115200 - and this loop is the one that retires edges. Measuring at
                // 60 s and comparing outlier rates is how the instrument's own effect gets ruled in or out.
#ifndef KIP_REPORT_S
#define KIP_REPORT_S 10
#endif
                static uint8_t reportIn = KIP_REPORT_S;
                if (--reportIn == 0) { reportIn = KIP_REPORT_S; measureReport(rig); }
#endif
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
