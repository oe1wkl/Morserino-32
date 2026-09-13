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

uint8_t   gBaseKey[32], gSessKey[32];
uint32_t  gSession = 0;
uint16_t  gTxSeq = 0;
IPAddress gPeer;
volatile uint8_t gSource = SRC_KEYER;

KeyerSession gKeyer;                 // touched ONLY by the send task, so it needs no lock

portMUX_TYPE      gStatMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool     gStatFresh = false;
volatile uint32_t gStatAtMs = 0;
bool              gShownLink = false;

inline uint32_t nowTicks() { return (uint32_t)(esp_timer_get_time() / 100); }

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

void onUdp(AsyncUDPPacket packet) {                 // WiFi core: verify and note that the rig is alive
    if (packet.length() > MAX_PACKET) return;
    Header h;
    if (!peekHeader(packet.data(), packet.length(), h)) return;
    if (h.type != PKT_STATS || h.session != gSession) return;
    Stats s;
    if (!decodeStats(packet.data(), packet.length(), gSessKey, h, s)) return;
    portENTER_CRITICAL(&gStatMux);
    gStatAtMs = millis();
    gStatFresh = true;
    portEXIT_CRITICAL(&gStatMux);
}

/// The one place that touches the socket. Everything the keying path produces reaches it through gEdgeQ.
void sendTask(void*) {
    uint8_t buf[MAX_PACKET];
    while (gRunTask) {
        QEdge e;
        int32_t until = tickDiff(gKeyer.nextSendAt(), nowTicks());
        uint32_t waitMs = (until > 0) ? ticksToMs((uint32_t)until) + 1 : 0;
        if (waitMs > 50) waitMs = 50;               // stay responsive to end()
        bool haveEdge = (xQueueReceive(gEdgeQ, &e, pdMS_TO_TICKS(waitMs)) == pdTRUE);
        uint8_t wpm = (gSource == SRC_KEYER) ? (uint8_t)MorsePreferences::wpm : 0;

        if (haveEdge) {
            gKeyer.addEdge(e.t, e.state);
            size_t n = gKeyer.buildKey(buf, sizeof(buf), e.t, wpm, gSource, gSession, ++gTxSeq, gSessKey, true);
            if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, DEFAULT_PORT);
            gKeyer.noteSent(e.t);
        } else if (gKeyer.sendDue(nowTicks())) {
            uint32_t t = nowTicks();
            size_t n = gKeyer.buildKey(buf, sizeof(buf), t, wpm, gSource, gSession, ++gTxSeq, gSessKey, false);
            if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, DEFAULT_PORT);
            gKeyer.noteSent(t);
        }
    }
    gTask = nullptr;
    vTaskDelete(nullptr);
}

bool handshake(uint32_t& outSession, uint16_t& maxKeydownMs) {
    uint8_t buf[MAX_PACKET];
    uint8_t nonceC[8];
    for (int i = 0; i < 8; i++) nonceC[i] = (uint8_t)(esp_random() & 0xFF);

    volatile bool got = false;
    HelloAck ack;
    MorseWiFi::audp.onPacket([&](AsyncUDPPacket packet) {
        if (got || packet.length() > MAX_PACKET) return;
        Header h;
        HelloAck a;
        if (!decodeHelloAck(packet.data(), packet.length(), gBaseKey, h, a)) return;
        if (memcmp(a.nonceC, (const void*)nonceC, 8) != 0) return;   // not an answer to our HELLO
        ack = a;
        got = true;
    });

    for (int attempt = 0; attempt < 10 && !got; attempt++) {
        Hello hello;
        memcpy(hello.nonceC, nonceC, 8);
        hello.tNow = nowTicks();
        hello.wpm = (gSource == SRC_KEYER) ? (uint8_t)MorsePreferences::wpm : 0;
        hello.redundancy = 4;
        hello.source = gSource;
        Header h; h.session = 0; h.seq = (uint16_t)(attempt + 1);
        size_t n = encodeHello(buf, sizeof(buf), h, hello, gBaseKey);
        if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, DEFAULT_PORT);
        for (int w = 0; w < 50 && !got; w++) delay(10);              // 500 ms between tries (spec §6.3)
    }
    if (!got) return false;

    deriveSessionKey(gBaseKey, ack.nonceC, ack.nonceS, gSessKey);
    outSession = ack.session;
    maxKeydownMs = ack.maxKeydownMs;
    return true;
}

void sendBye() {
    if (!gSession) return;
    uint8_t buf[MAX_PACKET];
    for (int i = 0; i < 3; i++) {                   // spec §6.6
        Header h; h.session = gSession; h.seq = ++gTxSeq;
        size_t n = encodeBye(buf, sizeof(buf), h, gSessKey);
        if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, DEFAULT_PORT);
        delay(100);
    }
}

} // namespace

// ================================================================ interface

void MorseKipKeyer::noteEdge(bool down) {
    if (!gActive || !gEdgeQ) return;
    QEdge e;
    e.t = nowTicks();                       // timestamped here, where the local TX line would have moved
    e.state = down ? KEY_DOWN : KEY_UP;
    xQueueSend(gEdgeQ, &e, 0);              // never waits: the keying path must not block (spec §10)
}

bool MorseKipKeyer::linked() {
    if (!gActive) return false;
    bool fresh; uint32_t at;
    portENTER_CRITICAL(&gStatMux);
    fresh = gStatFresh; at = gStatAtMs;
    portEXIT_CRITICAL(&gStatMux);
    return fresh && (millis() - at) < 5000;        // spec §10: "no link" after five seconds without STATS
}

void MorseKipKeyer::tick() {
    if (!gActive) return;
    // The operator may switch between iambic and straight key in the preferences mid-session (spec §6.4).
    gSource = (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY) ? SRC_STRAIGHT : SRC_KEYER;
    bool link = linked();
    if (link != gShownLink) {               // only on a change: a redraw is longer than a dit, so never routinely
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

    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, REGULAR, 0, "Connecting...");
    MorseOutput::refreshDisplay();
    if (protocolActive())
        MorseJSON::jsonCreate("message", "Connecting...", "");
    if (!MorseWiFi::wifiConnect()) return false;    // shows its own failure screen
    WiFi.setSleep(false);

    // Resolve once, here, so no DNS lookup can ever land on the keying path.
    String host = MorsePreferences::wlanTRXPeer;
    if (!gPeer.fromString(host.c_str()) && WiFi.hostByName(host.c_str(), gPeer) != 1) {
        showError("Host not found", host, "");
        return false;
    }

    MorseWiFi::audp.listen(DEFAULT_PORT);
    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, REGULAR, 0, "Calling rig...");
    MorseOutput::printOnScroll(1, REGULAR, 0, gPeer.toString());
    MorseOutput::refreshDisplay();

    uint16_t maxKeydown = 0;
    if (!handshake(gSession, maxKeydown)) {
        MorseWiFi::audp.close();
        gSession = 0;
        showError("No answer", "Check rig and", "pass phrase");
        return false;
    }
    MorseWiFi::audp.onPacket(onUdp);                // replaces the handshake handler

    gEdgeQ = xQueueCreate(64, sizeof(QEdge));
    if (!gEdgeQ) {
        MorseWiFi::audp.close();
        gSession = 0;
        showError("Out of memory", "", "");
        return false;
    }
    gKeyer.begin(4);
    gKeyer.noteSent(nowTicks());                    // arm the send schedule (D13)

    portENTER_CRITICAL(&gStatMux);                  // the HELLO_ACK itself is proof of a live link
    gStatFresh = true;
    gStatAtMs = millis();
    portEXIT_CRITICAL(&gStatMux);
    gShownLink = true;

    gRunTask = true;
    xTaskCreatePinnedToCore(sendTask, "kipkeyer", 4096, nullptr, 3, &gTask, 0);   // core 0: WiFi already lives there
    gActive = true;

    MorseMenu::showStartDisplay("", "Remote Keyer", gPeer.toString(), 1000);
    return true;
}

void MorseKipKeyer::end() {
    if (!gActive && !gTask && !gSession) return;    // nothing running: the usual case, from menu_()
    gActive = false;
    gRunTask = false;
    for (int i = 0; i < 50 && gTask; i++) delay(10);   // let the send task retire before the socket goes
    sendBye();
    MorseWiFi::audp.close();
    if (gEdgeQ) { vQueueDelete(gEdgeQ); gEdgeQ = nullptr; }
    gSession = 0;
    gShownLink = false;
    portENTER_CRITICAL(&gStatMux);
    gStatFresh = false;
    portEXIT_CRITICAL(&gStatMux);
    if (protocolActive())
        MorseJSON::jsonCreate("message", "Remote Keyer ended", "");
}

#endif // CONFIG_M32KIP
