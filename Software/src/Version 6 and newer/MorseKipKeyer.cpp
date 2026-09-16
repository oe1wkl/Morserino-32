/******************************************************************************************************************************
 *  M32KIP Keyer unit. See MorseKipKeyer.h and devdocs/m32kip/SPEC.md §6.4, §9, §10.
 *****************************************************************************************************************************/

#include "MorseKipKeyer.h"
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

extern void updateTopLine();

namespace {

struct QEdge { uint32_t t; uint8_t state; };

QueueHandle_t gEdgeQ   = nullptr;
TaskHandle_t  gTask    = nullptr;
volatile bool gActive  = false;      // gates noteEdge(): the hook in keyOut() is on every mode's path
volatile bool gRunTask = false;

uint8_t   gBaseKey[32];

// A reconnect replaces the session from the UDP callback while the send task may be using it on the other
// core, so "which session, under which key" is only ever read or written under this lock.
portMUX_TYPE  gSessMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t       gSessKey[32];
uint32_t      gSession = 0;
volatile bool gNewSession = false;   // set when a reconnect lands; the send task restarts its schedule

// ---- the rig's settings, cached here and adjusted from the preferences menu (D17) ----
//
// The socket belongs to the send task, so nothing here transmits: a request is flagged and the task sends it. The
// cache is what the menu displays, which is why it opens without waiting for a round trip.
const uint8_t RIGCFG_COUNT = 6;
const prefPos RIGCFG_PREFS[RIGCFG_COUNT] = {
    posKipPlayout, posKipMaxKeyer, posKipMaxManual, posKipFirstExt, posKipHangUnit, posKipHang
};
portMUX_TYPE     gCfgMux = portMUX_INITIALIZER_UNLOCKED;
RigCfgMsg        gCfgCache;                 // what the rig last reported (or what we just asked it for)
volatile bool    gCfgCapable = false;       // the rig advertised ACK_CFG_CAPABLE
volatile bool    gCfgReqDue  = false;       // send a CFG_REQ when the task next runs
volatile bool    gCfgSetDue  = false;       // ... or a CFG_SET, from gCfgCache
volatile bool    gCfgWaiting = false;       // a set is out and unacknowledged

uint8_t cfgField(const RigCfgMsg& m, uint8_t i) {
    switch (i) {
        case 0: return m.playout;
        case 1: return m.limitKeyer;
        case 2: return m.limitManual;
        case 3: return m.firstExt;
        case 4: return m.hangUnit;
        default: return m.hang;
    }
}

void cfgSetField(RigCfgMsg& m, uint8_t i, uint8_t v) {
    switch (i) {
        case 0: m.playout = v; break;
        case 1: m.limitKeyer = v; break;
        case 2: m.limitManual = v; break;
        case 3: m.firstExt = v; break;
        case 4: m.hangUnit = v; break;
        default: m.hang = v; break;
    }
}

enum { LINK_UP = 0, LINK_CALLING = 1 };
volatile uint8_t gLink = LINK_UP;
volatile bool    gByeSeen = false;
uint8_t          gNonceC[8];
volatile bool    gNonceValid = false;

uint16_t  gTxSeq = 0;                // send task only; end() runs after the task has retired
IPAddress gPeer;
volatile uint8_t gSource = SRC_KEYER;
uint8_t   gLastNoted = KEY_UP;       // loop context only

KeyerSession gKeyer;                 // send task only, so it needs no lock

portMUX_TYPE      gStatMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool     gStatFresh = false;
volatile uint32_t gStatAtMs = 0;
bool              gShownLink = false;

inline uint32_t nowTicks() { return (uint32_t)(esp_timer_get_time() / 100); }

void snapshotSession(uint32_t& session, uint8_t key[32]) {
    portENTER_CRITICAL(&gSessMux);
    session = gSession;
    memcpy(key, gSessKey, 32);
    portEXIT_CRITICAL(&gSessMux);
}

void markAlive() {
    portENTER_CRITICAL(&gStatMux);
    gStatFresh = true;
    gStatAtMs = millis();
    portEXIT_CRITICAL(&gStatMux);
}

bool heardRecently() {                              // spec §10: "no link" after five seconds without STATS
    bool fresh; uint32_t at;
    portENTER_CRITICAL(&gStatMux);
    fresh = gStatFresh; at = gStatAtMs;
    portEXIT_CRITICAL(&gStatMux);
    return fresh && (millis() - at) < 5000;
}

void showError(const String& l0, const String& l1, const String& l2) {
    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, l0);
    if (l1.length()) MorseOutput::printOnScroll(1, REGULAR, 0, l1);
    if (l2.length()) MorseOutput::printOnScroll(2, REGULAR, 0, l2);
    MorseOutput::refreshDisplay();
    if (protocolActive())
        MorseJSON::jsonCreate("message", "Remote Keyer: " + l0, "");
    delay(2500);
}

void sendHello(uint16_t seq, const uint8_t nonce[8]) {
    uint8_t buf[MAX_PACKET];
    Hello hello;
    memcpy(hello.nonceC, nonce, 8);
    hello.tNow = nowTicks();
    hello.wpm = (gSource == SRC_KEYER) ? (uint8_t)MorsePreferences::wpm : 0;
    hello.redundancy = 4;
    hello.source = gSource;
    Header h; h.session = 0; h.seq = seq;
    size_t n = encodeHello(buf, sizeof(buf), h, hello, gBaseKey);
    if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, MorsePreferences::kipPort);
}

/// Runs for the whole mode. HELLO_ACK is only accepted while calling and only for our current nonce; STATS
/// and BYE only for the current session.
void onUdp(AsyncUDPPacket packet) {
    size_t len = packet.length();
    if (len > MAX_PACKET) return;
    const uint8_t* data = packet.data();
    Header h;
    if (!peekHeader(data, len, h)) return;

    if (h.type == PKT_HELLO_ACK) {
        if (gLink != LINK_CALLING || !gNonceValid) return;
        HelloAck a;
        if (!decodeHelloAck(data, len, gBaseKey, h, a)) return;
        if (memcmp(a.nonceC, gNonceC, 8) != 0) return;         // an answer to an older HELLO
        uint8_t k[32];
        deriveSessionKey(gBaseKey, a.nonceC, a.nonceS, k);
        portENTER_CRITICAL(&gSessMux);
        memcpy(gSessKey, k, 32);
        gSession = a.session;
        portEXIT_CRITICAL(&gSessMux);
        gNonceValid = false;
        markAlive();
        gNewSession = true;
        gCfgCapable = (a.flags & ACK_CFG_CAPABLE) != 0;   // a reconnect may land on a different rig (D17)
        if (gCfgCapable) gCfgReqDue = true;               // refresh the cache for the preferences menu
        gLink = LINK_UP;
        return;
    }

    uint32_t session; uint8_t key[32];
    snapshotSession(session, key);
    if (session == 0 || h.session != session) return;
    if (h.type == PKT_STATS) {
        Stats st;
        if (decodeStats(data, len, key, h, st)) markAlive();
    } else if (h.type == PKT_CFG_VAL) {
        // The rig's own account of its settings - the answer to a fetch, or the acknowledgement of a set saying
        // what was actually stored after its own clamping. Either way it is the truth and the cache follows it.
        RigCfgMsg m;
        if (decodeCfg(data, len, key, h, m)) {
            portENTER_CRITICAL(&gCfgMux);
            gCfgCache = m;
            portEXIT_CRITICAL(&gCfgMux);
            gCfgWaiting = false;
            markAlive();
        }
    } else if (h.type == PKT_BYE) {
        if (decodeBye(data, len, key, h)) gByeSeen = true;      // the Rig ended it: call again straight away
    }
}

/// The one place that touches the socket. It also owns reconnecting (D14): the Rig ending a session, or five
/// seconds without a word from it, puts the link back to calling, and the Keyer HELLOs once a second until a
/// Rig answers. None of that ever runs on the keying path, so the operator keys on undisturbed throughout.
GlitchFilter      gFilter;                   // send task only
volatile uint32_t gGlitchTicks = 0;          // from the loop: Glitch Filter for a straight key, 0 for the iambic keyer

/// Every captured transition goes through the glitch filter before it can reach the wire (spec §10.1). The queue is
/// drained completely before anything is popped, and `now` is read before draining, so an edge's fate is decided by
/// the edges captured after it and never by how late this task happens to run.
uint32_t drainIntoFilter(TickType_t firstWait) {
    uint32_t now = nowTicks();
    QEdge e;
    if (xQueueReceive(gEdgeQ, &e, firstWait) == pdTRUE) {
        now = nowTicks();
        gFilter.push(e.t, e.state);
        while (xQueueReceive(gEdgeQ, &e, 0) == pdTRUE) gFilter.push(e.t, e.state);
    }
    return now;
}

void sendTask(void*) {
    uint8_t  buf[MAX_PACKET];
    uint32_t nextHelloMs = 0;
    uint16_t helloSeq = 0;
    while (gRunTask) {
        gFilter.setWidth(gGlitchTicks);
        if (gLink == LINK_UP) {
            if (gNewSession) {                      // a reconnect has just landed: start the schedule afresh
                gNewSession = false;
                gKeyer.begin(4);
                gKeyer.noteSent(nowTicks());
            }
            if (gByeSeen || !heardRecently()) {
                gByeSeen = false;
                gLink = LINK_CALLING;
                nextHelloMs = 0;
                continue;
            }
        }
        uint32_t ft; uint8_t fs;
        if (gLink == LINK_CALLING) {
            // Keying goes on locally; with no session it goes nowhere. It still passes the filter, so the filter's
            // idea of the key level is right when the link comes back.
            uint32_t now = drainIntoFilter(pdMS_TO_TICKS(50));
            while (gFilter.pop(now, ft, fs)) {}
            if (millis() >= nextHelloMs) {
                gNonceValid = false;
                for (int i = 0; i < 8; i++) gNonceC[i] = (uint8_t)(esp_random() & 0xFF);
                gNonceValid = true;
                sendHello(++helloSeq, gNonceC);
                nextHelloMs = millis() + 1000;
            }
            continue;
        }

        int32_t until = tickDiff(gKeyer.nextSendAt(), nowTicks());
        if (gFilter.pending()) {                    // an edge waiting out the filter may be due before the next repeat
            int32_t rel = tickDiff(gFilter.releaseAt(), nowTicks());
            if (rel < until) until = rel;
        }
        uint32_t waitMs = (until > 0) ? ticksToMs((uint32_t)until) + 1 : 0;
        if (waitMs > 50) waitMs = 50;               // stay responsive to end() and to the link state
        uint32_t now = drainIntoFilter(pdMS_TO_TICKS(waitMs));
        uint8_t wpm = (gSource == SRC_KEYER) ? (uint8_t)MorsePreferences::wpm : 0;
        uint32_t session; uint8_t key[32];
        snapshotSession(session, key);

        // Remote configuration goes out from here, where the socket lives - never from the keying path (D17).
        // A fetch or a set is a single small packet between edges; if one is lost the operator sees the value
        // unchanged and can try again, which is why there is no retry machinery.
        if ((gCfgReqDue || gCfgSetDue) && session) {
            uint8_t cbuf[MAX_PACKET];
            Header ch; ch.session = session; ch.seq = ++gTxSeq;
            if (gCfgSetDue) {
                RigCfgMsg m;
                portENTER_CRITICAL(&gCfgMux); m = gCfgCache; portEXIT_CRITICAL(&gCfgMux);
                size_t n = encodeCfgSet(cbuf, sizeof(cbuf), ch, m, key);
                if (n) MorseWiFi::audp.writeTo(cbuf, n, gPeer, MorsePreferences::kipPort);
                gCfgSetDue = false;
            } else {
                size_t n = encodeCfgReq(cbuf, sizeof(cbuf), ch, key);
                if (n) MorseWiFi::audp.writeTo(cbuf, n, gPeer, MorsePreferences::kipPort);
                gCfgReqDue = false;
            }
        }

        bool sent = false;
        while (gFilter.pop(now, ft, fs)) {
            gKeyer.addEdge(ft, fs);
            size_t n = gKeyer.buildKey(buf, sizeof(buf), ft, wpm, gSource, session, ++gTxSeq, key, true);
            if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, MorsePreferences::kipPort);
            gKeyer.noteSent(ft);
            sent = true;
        }
        if (!sent && gKeyer.sendDue(nowTicks())) {
            uint32_t t = nowTicks();
            size_t n = gKeyer.buildKey(buf, sizeof(buf), t, wpm, gSource, session, ++gTxSeq, key, false);
            if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, MorsePreferences::kipPort);
            gKeyer.noteSent(t);
        }
    }
    gTask = nullptr;
    vTaskDelete(nullptr);
}

/// The first call, at entry. Blocking is fine here - the mode has not started - and a Rig that does not
/// answer ten tries in a row is reported, so a wrong address or pass phrase is not mistaken for a dropout.
bool firstHandshake() {
    uint8_t nonceC[8];
    for (int i = 0; i < 8; i++) nonceC[i] = (uint8_t)(esp_random() & 0xFF);
    volatile bool got = false;
    HelloAck ack;
    MorseWiFi::audp.onPacket([&](AsyncUDPPacket packet) {
        if (got || packet.length() > MAX_PACKET) return;
        Header h;
        HelloAck a;
        if (!decodeHelloAck(packet.data(), packet.length(), gBaseKey, h, a)) return;
        if (memcmp(a.nonceC, (const void*)nonceC, 8) != 0) return;
        ack = a;
        got = true;
    });
    for (int attempt = 0; attempt < 10 && !got; attempt++) {
        sendHello((uint16_t)(attempt + 1), nonceC);
        for (int w = 0; w < 50 && !got; w++) delay(10);              // 500 ms between tries (spec §6.3)
    }
    if (!got) return false;
    uint8_t k[32];
    deriveSessionKey(gBaseKey, ack.nonceC, ack.nonceS, k);
    portENTER_CRITICAL(&gSessMux);
    memcpy(gSessKey, k, 32);
    gSession = ack.session;
    portEXIT_CRITICAL(&gSessMux);
    // Does this rig answer configuration requests (D17)? An older one never sets the bit, and the preferences then
    // simply do not offer the rig items. Fetch straight away, so the menu has real values before it is first opened.
    gCfgCapable = (ack.flags & ACK_CFG_CAPABLE) != 0;
    gCfgReqDue = gCfgCapable;
    return true;
}

void sendBye() {
    uint32_t session; uint8_t key[32];
    snapshotSession(session, key);
    if (!session || gLink != LINK_UP) return;       // nothing established to say goodbye to
    uint8_t buf[MAX_PACKET];
    for (int i = 0; i < 3; i++) {                   // spec §6.6
        Header h; h.session = session; h.seq = ++gTxSeq;
        size_t n = encodeBye(buf, sizeof(buf), h, key);
        if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, MorsePreferences::kipPort);
        delay(100);
    }
}

} // namespace

// ================================================================ interface

void MorseKipKeyer::noteEdge(bool down) {
    if (!gActive || !gEdgeQ) return;
    uint8_t state = down ? KEY_DOWN : KEY_UP;
    // keyOut() is also called just to make sure the key is off - when a memory stops, when a setting changes -
    // so the same state can arrive twice. Only a real transition is an edge. Anything else reaches the Rig
    // as a malformed stream, and a burst of those now ends the session (spec §8).
    if (state == gLastNoted) return;
    gLastNoted = state;
    QEdge e;
    e.t = nowTicks();                       // timestamped here, where the local TX line would have moved
    e.state = state;
    xQueueSend(gEdgeQ, &e, 0);              // never waits: the keying path must not block (spec §10)
}

bool MorseKipKeyer::linked() {
    return gActive && gLink == LINK_UP && heardRecently();
}

// ---- the rig's settings, seen and changed from the operating position (D17) ----

bool MorseKipKeyer::rigCfgAvailable() {
    return linked() && gCfgCapable;
}

uint8_t MorseKipKeyer::rigCfgCount() {
    return RIGCFG_COUNT;
}

prefPos MorseKipKeyer::rigCfgPref(uint8_t index) {
    return RIGCFG_PREFS[index < RIGCFG_COUNT ? index : 0];
}

uint8_t MorseKipKeyer::rigCfgValue(uint8_t index) {
    if (index >= RIGCFG_COUNT) return 0;
    RigCfgMsg m;
    portENTER_CRITICAL(&gCfgMux); m = gCfgCache; portEXIT_CRITICAL(&gCfgMux);
    return cfgField(m, index);
}

void MorseKipKeyer::rigCfgFetch() {
    if (rigCfgAvailable()) gCfgReqDue = true;           // the send task transmits it; this never blocks
}

void MorseKipKeyer::rigCfgSet(uint8_t index, uint8_t value) {
    if (index >= RIGCFG_COUNT || !rigCfgAvailable()) return;
    portENTER_CRITICAL(&gCfgMux);
    cfgSetField(gCfgCache, index, value);               // the menu reads this back at once, so the knob feels live
    portEXIT_CRITICAL(&gCfgMux);
    gCfgWaiting = true;                                 // ... and the rig's reply confirms what it really stored
    gCfgSetDue = true;
}

bool MorseKipKeyer::rigCfgPending() {
    return gCfgWaiting || gCfgSetDue;
}

void MorseKipKeyer::tick() {
    if (!gActive) return;
    gSource = (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY) ? SRC_STRAIGHT : SRC_KEYER;
    // Contact bounce is a straight key's problem (spec §10.1). The iambic keyer's edges are generated, clean by
    // construction, and must not pay the filter's delay.
    gGlitchTicks = (gSource == SRC_STRAIGHT) ? msToTicks(MorsePreferences::pliste[posKipGlitch].value) : 0;
    bool link = linked();
    if (link != gShownLink) {               // only on a change: a redraw is longer than a dit
        gShownLink = link;
        updateTopLine();
    }
}

bool MorseKipKeyer::begin() {
    end();                                  // never two sessions

    if (MorsePreferences::kipPsk.length() < 12) {
        showError("No key set", "Set a pass-", "phrase first");
        return false;
    }
    if (MorsePreferences::wlanTRXPeer.length() == 0) {
        showError("No rig host", "Set TRX Peer", "in Config WiFi");
        return false;
    }
    deriveBaseKey(MorsePreferences::kipPsk.c_str(), gBaseKey);
    gSource = (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY) ? SRC_STRAIGHT : SRC_KEYER;
    gLastNoted = KEY_UP;
    gFilter.reset();                                // the send task is not running yet (end() above retired it)
    // A fresh session knows nothing about the rig yet: no capability, no values, nothing outstanding. Carrying any
    // of that over would show a previous rig's settings, or leave a set waiting for an answer that never comes.
    gCfgCapable = false;
    gCfgReqDue = gCfgSetDue = gCfgWaiting = false;
    portENTER_CRITICAL(&gCfgMux); gCfgCache = RigCfgMsg(); portEXIT_CRITICAL(&gCfgMux);

    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, REGULAR, 0, "Connecting...");
    MorseOutput::refreshDisplay();
    if (protocolActive())
        MorseJSON::jsonCreate("message", "Connecting...", "");
    if (!MorseWiFi::wifiConnect()) return false;
    WiFi.setSleep(false);

    String host = MorsePreferences::wlanTRXPeer;    // resolved once, so no DNS lookup lands on the keying path
    if (!gPeer.fromString(host.c_str()) && WiFi.hostByName(host.c_str(), gPeer) != 1) {
        showError("Host not found", host, "");
        return false;
    }

    MorseWiFi::audp.listen(MorsePreferences::kipPort);
    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, REGULAR, 0, "Calling rig...");
    MorseOutput::printOnScroll(1, REGULAR, 0, gPeer.toString());
    MorseOutput::refreshDisplay();

    gLink = LINK_UP;
    if (!firstHandshake()) {
        MorseWiFi::audp.close();
        portENTER_CRITICAL(&gSessMux); gSession = 0; portEXIT_CRITICAL(&gSessMux);
        showError("No answer", "Check rig and", "pass phrase");
        return false;
    }
    MorseWiFi::audp.onPacket(onUdp);                // replaces the handshake handler for the rest of the mode

    gEdgeQ = xQueueCreate(64, sizeof(QEdge));
    if (!gEdgeQ) {
        MorseWiFi::audp.close();
        portENTER_CRITICAL(&gSessMux); gSession = 0; portEXIT_CRITICAL(&gSessMux);
        showError("Out of memory", "", "");
        return false;
    }
    gKeyer.begin(4);
    gKeyer.noteSent(nowTicks());                    // arm the send schedule (D13)
    gNewSession = false;
    gByeSeen = false;
    markAlive();                                    // the HELLO_ACK itself is proof of a live link
    gShownLink = true;

    gRunTask = true;
    xTaskCreatePinnedToCore(sendTask, "kipkeyer", 4096, nullptr, 3, &gTask, 0);   // core 0: WiFi lives there
    gActive = true;

    MorseMenu::showStartDisplay("", "Remote Keyer", gPeer.toString(), 1000);
    return true;
}

void MorseKipKeyer::end() {
    uint32_t session;
    portENTER_CRITICAL(&gSessMux); session = gSession; portEXIT_CRITICAL(&gSessMux);
    if (!gActive && !gTask && !session) return;     // nothing running: the usual case, from menu_()
    gActive = false;
    gRunTask = false;
    for (int i = 0; i < 50 && gTask; i++) delay(10);   // let the send task retire before the socket goes
    sendBye();
    MorseWiFi::audp.close();
    if (gEdgeQ) { vQueueDelete(gEdgeQ); gEdgeQ = nullptr; }
    portENTER_CRITICAL(&gSessMux); gSession = 0; portEXIT_CRITICAL(&gSessMux);
    gLink = LINK_UP;
    gByeSeen = false;
    gNonceValid = false;
    gShownLink = false;
    // Nothing about the rig outlives the mode: a stale gCfgWaiting would leave rigCfgPending() true for good, and
    // the preferences menu waits on it before letting keying resume (D17).
    gCfgCapable = false;
    gCfgReqDue = gCfgSetDue = gCfgWaiting = false;
    portENTER_CRITICAL(&gStatMux); gStatFresh = false; portEXIT_CRITICAL(&gStatMux);
    if (protocolActive())
        MorseJSON::jsonCreate("message", "Remote Keyer ended", "");
}

#endif // CONFIG_M32KIP
