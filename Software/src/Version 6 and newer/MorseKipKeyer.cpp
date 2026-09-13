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

// ---- from m32_v6.ino ----
extern int          checkEncoder();
extern void         serialEvent();
extern boolean      checkPaddles();
extern boolean      doPaddleIambic(boolean, boolean);
extern boolean      leftKey, rightKey;
extern KEYERSTATES  keyerState;
extern void         clearPaddleLatches();
extern void         displayCWspeed();
extern void         changeSpeed(int t);
extern void         changeVolume(int t);
extern encoderMode  encoderState;
extern boolean      goToMenu;
extern boolean      speedChanged;

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
uint8_t   gSource = SRC_KEYER;

KeyerSession gKeyer;                 // touched ONLY by the send task, so it needs no lock

// ---- what the Rig tells us, for the display ----
portMUX_TYPE gStatMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool     gStatFresh = false;
volatile uint32_t gStatAtMs = 0;
Stats             gStat;
uint16_t          gMaxKeydownMs = 0;

inline uint32_t nowTicks() { return (uint32_t)(esp_timer_get_time() / 100); }

void onUdp(AsyncUDPPacket packet) {                 // WiFi core: verify and stash, nothing more
    if (packet.length() > MAX_PACKET) return;
    Header h;
    if (!peekHeader(packet.data(), packet.length(), h)) return;
    if (h.type != PKT_STATS || h.session != gSession) return;
    Stats s;
    if (!decodeStats(packet.data(), packet.length(), gSessKey, h, s)) return;
    portENTER_CRITICAL(&gStatMux);
    gStat = s;
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
        if (waitMs > 50) waitMs = 50;               // keep the task responsive to shutdown
        bool haveEdge = (xQueueReceive(gEdgeQ, &e, pdMS_TO_TICKS(waitMs)) == pdTRUE);

        if (haveEdge) {
            gKeyer.addEdge(e.t, e.state);
            size_t n = gKeyer.buildKey(buf, sizeof(buf), e.t,
                                       (gSource == SRC_KEYER) ? (uint8_t)MorsePreferences::wpm : 0,
                                       gSource, gSession, ++gTxSeq, gSessKey, true);
            if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, DEFAULT_PORT);
            gKeyer.noteSent(e.t);
        } else if (gKeyer.sendDue(nowTicks())) {
            uint32_t t = nowTicks();
            size_t n = gKeyer.buildKey(buf, sizeof(buf), t,
                                       (gSource == SRC_KEYER) ? (uint8_t)MorsePreferences::wpm : 0,
                                       gSource, gSession, ++gTxSeq, gSessKey, false);
            if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, DEFAULT_PORT);
            gKeyer.noteSent(t);
        }
    }
    gTask = nullptr;
    vTaskDelete(nullptr);
}

bool handshake(uint32_t& outSession) {
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
    gMaxKeydownMs = ack.maxKeydownMs;
    return true;
}

void sendBye() {
    uint8_t buf[MAX_PACKET];
    for (int i = 0; i < 3; i++) {
        Header h; h.session = gSession; h.seq = ++gTxSeq;
        size_t n = encodeBye(buf, sizeof(buf), h, gSessKey);
        if (n) MorseWiFi::audp.writeTo(buf, n, gPeer, DEFAULT_PORT);
        delay(100);
    }
}

} // namespace

// ---------------------------------------------------------------- the hook on the keying path

void MorseKipKeyer::noteEdge(bool down) {
    if (!gActive || !gEdgeQ) return;
    QEdge e;
    e.t = nowTicks();                       // timestamped here, where the local TX line would have moved
    e.state = down ? KEY_DOWN : KEY_UP;
    xQueueSend(gEdgeQ, &e, 0);              // never waits: the keying path must not block (spec §10)
}

// ================================================================ the mode

void MorseKipKeyer::run() {
    if (MorsePreferences::kipPsk.length() < 12) {
        MorseOutput::clearDisplay();
        MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, "No key set");
        MorseOutput::printOnScroll(1, REGULAR, 0, "Set a pass-");
        MorseOutput::printOnScroll(2, REGULAR, 0, "phrase first");
        delay(2500);
        return;
    }
    if (MorsePreferences::wlanTRXPeer.length() == 0) {
        MorseOutput::clearDisplay();
        MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, "No rig host");
        MorseOutput::printOnScroll(1, REGULAR, 0, "Set TRX Peer");
        MorseOutput::printOnScroll(2, REGULAR, 0, "in Config WiFi");
        delay(2500);
        return;
    }
    deriveBaseKey(MorsePreferences::kipPsk.c_str(), gBaseKey);
    gSource = (MorsePreferences::pliste[posCurtisMode].value == STRAIGHTKEY) ? SRC_STRAIGHT : SRC_KEYER;

    MorseMenu::showStartDisplay("Remote Keyer", "Connecting...", "", 600);
    if (!MorseWiFi::wifiConnect()) return;
    WiFi.setSleep(false);

    // Resolve the rig once, here, so nothing in the keying path ever waits on DNS (spec open question 3).
    String host = MorsePreferences::wlanTRXPeer;
    if (!gPeer.fromString(host.c_str())) {
        if (WiFi.hostByName(host.c_str(), gPeer) != 1) {
            MorseOutput::clearDisplay();
            MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, "Host not found");
            MorseOutput::printOnScroll(1, REGULAR, 0, host);
            delay(2500);
            return;
        }
    }

    MorseWiFi::audp.listen(DEFAULT_PORT);
    MorseOutput::clearDisplay();
    MorseOutput::printOnScroll(0, REGULAR, 0, "Calling rig...");
    MorseOutput::printOnScroll(1, REGULAR, 0, gPeer.toString());
    MorseOutput::refreshDisplay();

    if (!handshake(gSession)) {
        MorseOutput::clearDisplay();
        MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, "No answer");
        MorseOutput::printOnScroll(1, REGULAR, 0, "Check rig/key");
        delay(2500);
        MorseWiFi::audp.close();
        return;
    }
    MorseWiFi::audp.onPacket(onUdp);            // replaces the handshake handler

    gEdgeQ = xQueueCreate(64, sizeof(QEdge));
    if (!gEdgeQ) { MorseWiFi::audp.close(); return; }
    gKeyer.begin(4);
    gKeyer.noteSent(nowTicks());                // arm the send schedule (D13)
    gRunTask = true;
    // Pinned to core 0, where the WiFi stack already lives, so it never competes with the keyer for core 1.
    xTaskCreatePinnedToCore(sendTask, "kipkeyer", 4096, nullptr, 3, &gTask, 0);
    gActive = true;

    MorseOutput::clearDisplay();
    MorseOutput::printOnStatusLine(true, 0, "Keyer " + gPeer.toString());
    clearPaddleLatches();
    encoderState = speedSettingMode;
    displayCWspeed();
    MorseOutput::displayVolume(encoderState == speedSettingMode, MorsePreferences::sidetoneVolume);

    uint32_t lastDraw = 0, keyDownAt = 0, warnedAt = 0;
    bool wasDown = false, linkLost = false;

    for (;;) {
        serialEvent();                          // keep the protocol alive inside the mode
        if (goToMenu) { goToMenu = false; break; }

        Buttons::modeButton.Update();
        if (Buttons::modeButton.clicks == -1) break;
        if (Buttons::modeButton.clicks == 2) {
            MorsePreferences::setupPreferences(MorsePreferences::menuPtr);
            MorseOutput::clearDisplay();
            MorseOutput::printOnStatusLine(true, 0, "Keyer " + gPeer.toString());
            Buttons::modeButton.clicks = 0;
            lastDraw = 0;
        }
        Buttons::modeButton.clicks = 0;

        Buttons::volButton.Update();
        if (Buttons::volButton.clicks == 1) {   // the global speed/volume toggle (UX conventions §2)
            encoderState = (encoderState == speedSettingMode) ? volumeSettingMode : speedSettingMode;
            displayCWspeed();
            MorseOutput::displayVolume(encoderState == speedSettingMode, MorsePreferences::sidetoneVolume);
            Buttons::volButton.clicks = 0;
        }
        int t = checkEncoder();
        if (t != 0) {
            if (encoderState == speedSettingMode) changeSpeed(t); else changeVolume(t);
        }
        if (speedChanged) { speedChanged = false; displayCWspeed(); }

        // The keyer itself. keyOut() calls noteEdge() at each transition, so nothing here has to know
        // about the network at all - which is the point of doing it this way.
        switch (keyerState) {
            case DIT: case DAH: case KEY_START: break;
            default: checkPaddles(); break;
        }
        if (doPaddleIambic(leftKey, rightKey)) continue;     // busy keying: tight loop, no display, no waiting

        // Warn once when a mark passes half the limit the Rig told us it will enforce (spec §8).
        bool down = (leftKey || rightKey);
        if (down && !wasDown) { keyDownAt = millis(); warnedAt = 0; }
        if (down && gMaxKeydownMs && !warnedAt && (millis() - keyDownAt) > (uint32_t)(gMaxKeydownMs / 2)) {
            warnedAt = millis();
            MorseOutput::printOnStatusLine(true, 0, "KEY DOWN LIMIT");
        }
        if (!down && wasDown) lastDraw = 0;
        wasDown = down;

        // Display only in a gap: a redraw is longer than a dit, so it may never land on the keying path.
        bool idle = (keyerState == IDLE_STATE) && !down;
        if (idle && (millis() - lastDraw) > 250) {
            lastDraw = millis();
            Stats s;
            uint32_t at;
            portENTER_CRITICAL(&gStatMux);
            s = gStat; at = gStatAtMs;
            portEXIT_CRITICAL(&gStatMux);
            bool haveLink = gStatFresh && (millis() - at) < 5000;
            if (haveLink != !linkLost) lastDraw = 0;
            linkLost = !haveLink;
            MorseOutput::clearScrollLines();
            if (!haveLink) {
                MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, "No link");
                MorseOutput::printOnScroll(1, REGULAR, 0, "rig silent");
            } else {
                MorseOutput::printOnScroll(0, REGULAR, 0, String(s.rigState & RIG_KEY_DOWN ? "TX " : "   ")
                                                          + "D" + String(s.playoutMs) + "ms");
                MorseOutput::printOnScroll(1, REGULAR, 0, "loss " + String(s.lossPct) + "%  late "
                                                          + String(s.lateEdges));
                MorseOutput::printOnScroll(2, REGULAR, 0, "jit "
                                                          + String(s.jitter100us / 10) + "ms");
            }
            MorseOutput::refreshDisplay();
        }
    }

    // ---- leaving ----
    gActive = false;
    gRunTask = false;
    for (int i = 0; i < 50 && gTask; i++) delay(10);      // let the send task retire before the socket goes
    sendBye();
    MorseWiFi::audp.close();
    if (gEdgeQ) { vQueueDelete(gEdgeQ); gEdgeQ = nullptr; }
    gSession = 0;
    if (protocolActive())
        MorseJSON::jsonCreate("message", "Remote Keyer ended", "");
}

#endif // CONFIG_M32KIP
