/******************************************************************************************************************************
 *  Software for the Morserino-32 (M32) multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module   ***
 *  Copyright (C) 2018-2025  Willi Kraml, OE1WKL                                                                            ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program.
 *  If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

#ifdef CONFIG_BLE_SERIAL

#include "MorseBleSerial.h"
#include "BleByteRing.h"
#include "M32ProtocolOut.h"      // bleProtocol
#include "MorseJSON.h"           // jsonError on init failure
#include "MorseOutput.h"         // splash on init failure
#include "morsedefs.h"

#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLE2902.h"
#include <esp_bt.h>              // esp_bt_controller_get_status
#include <esp_bt_main.h>         // esp_bluedroid_get_status

// Nordic UART Service: the de-facto BLE-UART standard — works out of the box
// with CoreBluetooth, bleak, nRF Connect. RX is written by the central (our
// command input), TX notifies the central (our protocol output).
static const char *NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *NUS_RX_UUID      = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *NUS_TX_UUID      = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *BLE_NAME         = "Morserino-32";   // distinct from the HID's "Morserino32 Keyboard";
                                                        // carried in the scan response (no room next to the 128-bit UUID)

volatile bool MorseBleSerial::isRunning = false;
volatile bool MorseBleSerial::isConnected = false;

static volatile bool sessionResetPending = false;
static volatile bool advertisePending = false;
static volatile bool lineResetLatch = false;            // pump() -> bleSerialEvent(): discard your partial line
static volatile uint16_t mtuPayload = 20;               // usable notify payload; 20 until the MTU event fires

static BleByteRing<1024> rxRing;                        // producer: onWrite (Bluedroid task); consumer: loop task
static BleByteRing<4096> txRing;                        // produced AND consumed on the loop task
static uint8_t staging[512];                            // contiguous chunk for notify()
static BleFlowGate flow;
static uint32_t txDropped = 0;                          // diagnostic: protocol bytes dropped on a stalled client
static bool txBackoff = false;                          // ring filled and the drain deadline passed: drop instead of
                                                        // waiting, so a multi-KB response stalls the loop task at most
                                                        // once (~20 ms) per congestion episode, not once per byte

static BLEServer *bleServer = nullptr;
static BLECharacteristic *txChar = nullptr;
static volatile uint16_t ourGattsIf = 0xFFFF;           // hook filter: never let another server's events
static volatile uint16_t ourConnId = 0xFFFF;            // (e.g. HID, should the exclusion ever weaken) credit our pump

// ---------------------------------------------------------------- callbacks —
// all run on the Bluedroid host task: set flags/marks, produce bytes, return.
// Never block here (the HID keyboard's delay(5000) in onDisconnect is the
// anti-pattern), never touch consumer-owned ring indices.

class BleSerialServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) override {
        ourConnId = server->getConnId();
        rxRing.latchMark();                             // serialized with onWrite on this task: cleanly
        mtuPayload = 20;                                // separates old-session bytes from the new session's;
        MorseBleSerial::isConnected = true;             // MTU is per-connection state — back to the ATT default
        sessionResetPending = true;                     // until this central's own MTU exchange (a stale large
    }                                                   // value would ATT-truncate notifies = silent byte loss)
    void onDisconnect(BLEServer *server) override {
        (void) server;
        MorseBleSerial::isConnected = false;
        bleProtocol = false;                            // session state dies with the link
        ourConnId = 0xFFFF;
        rxRing.latchMark();                             // re-latch at session END too: a stale connect-time mark
                                                        // would make pump()'s reset rewind tail over consumed
                                                        // bytes and REPLAY the whole session's commands
        sessionResetPending = true;
        advertisePending = true;                        // restart advertising from pump(), not from here
    }
    void onMtuChanged(BLEServer *server, esp_ble_gatts_cb_param_t *param) override {
        (void) server;
        uint16_t payload = (param->mtu.mtu > 23) ? (uint16_t)(param->mtu.mtu - 3) : 20;
        mtuPayload = (payload > sizeof(staging)) ? sizeof(staging) : payload;
    }
};

class BleSerialRxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *characteristic) override {
        // getData()/getLength(), NOT getValue(): no std::string heap churn on the BT task
        rxRing.produce(characteristic->getData(), characteristic->getLength());
        // all-or-nothing: an overflowed write is dropped whole and flagged, so
        // the consumer never dispatches a torn command line
    }
};

// notify flow control (PLAN D17): CONF fires per notification hand-off,
// CONGEST when Bluedroid's buffer pool backs up. BLECharacteristic::notify()
// itself never surfaces either, hence this hook.
static void bleSerialGattsHook(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    if (event == ESP_GATTS_REG_EVT) {
        // fires during our createServer(); mutual exclusion with the HID
        // keyboard guarantees no other app registers while BLE Serial is up
        ourGattsIf = gatts_if;
        return;
    }
    if (gatts_if != ourGattsIf)
        return;
    if (event == ESP_GATTS_CONF_EVT && param->conf.conn_id == ourConnId)
        flow.confCount = (uint16_t)(flow.confCount + 1);      // BT task is the sole writer of confCount
    else if (event == ESP_GATTS_CONGEST_EVT && param->congest.conn_id == ourConnId)
        flow.congested = param->congest.congested;
}

static BleSerialServerCallbacks serverCallbacks;        // static: no per-stop/start-cycle heap churn
static BleSerialRxCallbacks rxCallbacks;

// -------------------------------------------------------------- module API —
// everything below runs on the Arduino loop task.

bool MorseBleSerial::linkUp() {
    return isRunning && isConnected && !sessionResetPending;
}

static bool initFailedSticky = false;   // don't retry (and re-splash) every polling pass after a failure;
                                        // cleared by stop(), i.e. a pref off->on cycle or reboot allows retry

static bool bleInitFail(const char *why) {
    MorseOutput::printOnScroll(2, BOLD, 0, "BLE init fail");
    MorseJSON::jsonError(String("BLE INIT FAILED: ") + why);
    initFailedSticky = true;
    return false;
}

bool MorseBleSerial::init() {
    if (isRunning)
        return true;
    if (initFailedSticky)
        return false;
    // Defensive (PLAN D16): after a legacy deinit(true) the library's
    // 'initialized' flag stays true, init() becomes a silent no-op and
    // createServer() would block the loop task forever in registerApp.
    // Refuse visibly instead of freezing the device.
    if (BLEDevice::getInitialized() ||
        esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED)
        return bleInitFail("STACK STATE");

    BLEDevice::init(BLE_NAME);
    // BLEDevice::init returns void and swallows btStart()/bluedroid failures
    // (e.g. ESP_ERR_NO_MEM on a fragmented heap) while still setting its
    // 'initialized' flag; createServer() would then block forever waiting for
    // a REG_EVT that never comes. Verify the stack actually came up.
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED ||
        esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        BLEDevice::deinit(false);       // clear the library flag so a later retry is real
        return bleInitFail("NO MEM");
    }
    BLEDevice::setMTU(517);
    BLEDevice::setCustomGattsHandler(bleSerialGattsHook);

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(&serverCallbacks);

    BLEService *service = bleServer->createService(NUS_SERVICE_UUID);
    txChar = service->createCharacteristic(NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    txChar->addDescriptor(new BLE2902());
    BLECharacteristic *rxChar = service->createCharacteristic(NUS_RX_UUID,
                                    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    rxChar->setCallbacks(&rxCallbacks);
    service->start();

    // flags (3 B) + 128-bit NUS UUID (18 B) fill the 31-byte ADV PDU; the
    // device name rides in the scan response so scanners still show it
    BLEAdvertising *advertising = bleServer->getAdvertising();
    advertising->addServiceUUID(NUS_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->start();

    // no BLESecurity / no bonding (PLAN D4): same trust model as a USB cable,
    // and what keeps the iOS CoreBluetooth path prompt-free
    isRunning = true;
    return true;
}

void MorseBleSerial::stop() {
    // PLAN D19: synchronous teardown; never trust onDisconnect to be
    // delivered through deinit. Callers that want the TX ring delivered call
    // txFlush() BEFORE stop() — once isRunning drops, everything here is a
    // defined no-op, which is what makes remote self-disable (stop() reached
    // from inside the bleSerialEvent dispatch chain) safe.
    bool wasRunning = isRunning;
    isRunning = false;
    if (wasRunning)
        BLEDevice::deinit(false);       // deinit(true) poisons the library for the rest of the boot (PLAN D16)
    isConnected = false;
    bleProtocol = false;                // protocolActive() reverts immediately: auto-sleep keeps working
    initFailedSticky = false;           // a pref off->on cycle may retry a failed init
    sessionResetPending = false;
    advertisePending = false;
    lineResetLatch = false;
    txBackoff = false;
    ourGattsIf = 0xFFFF;
    ourConnId = 0xFFFF;
    mtuPayload = 20;
    rxRing.hardReset();                 // legal: the producer (GATT server) is down
    txRing.hardReset();
    flow.resetSession(millis());
    bleServer = nullptr;                // objects are leaked by the library across cycles (documented);
    txChar = nullptr;                   // our own callback instances are static, no churn from us
}

void MorseBleSerial::pump() {
    if (!isRunning)
        return;
    if (sessionResetPending) {          // consumer-side only (PLAN D18): tail -> producer-latched mark
        rxRing.resetToMark();           // (latched at BOTH session edges), old-session bytes vanish, a
        rxRing.overflow = false;        // new central's early first write (before this pass) survives
        rxRing.clearPoisoned();
        txRing.hardReset();             // TX ring is loop-task-owned on both sides: trivially safe
        flow.resetSession(millis());
        lineResetLatch = true;          // bleSerialEvent must drop its partial line
        bleProtocol = false;            // belt-and-braces: a handshake racing the disconnect edge must
                                        // not leave a dead session marked active (zombie sleep-suppression)
        txBackoff = false;
        sessionResetPending = false;
    }
    if (advertisePending && !isConnected && bleServer != nullptr) {
        bleServer->getAdvertising()->start();
        advertisePending = false;
    }
    flow.service(millis());             // watchdog: resync credits if confirmations stopped
    if (txBackoff && txRing.used() == 0)
        txBackoff = false;              // congestion episode over: ring fully drained
    uint8_t chunks = 0;
    while (chunks < 2 && linkUp() && flow.canSend() && txRing.used() > 0) {
        uint16_t n = bleStageChunk(txRing, staging, mtuPayload);
        if (n == 0)
            break;
        txChar->setValue(staging, n);
        txChar->notify();               // returns void on error too — flow control paces us regardless
        flow.onNotified();
        chunks++;
    }
}

bool MorseBleSerial::readByte(uint8_t &b) {
    if (!isRunning)
        return false;
    return rxRing.consume(b);
}

bool MorseBleSerial::takeLineReset() {
    if (!lineResetLatch)
        return false;
    lineResetLatch = false;
    return true;
}

bool MorseBleSerial::takeRxOverflow() {
    // call only with the ring drained (readByte just returned false): taking
    // the flag also lifts the poison so the producer admits writes again
    if (!rxRing.overflow)
        return false;
    rxRing.overflow = false;
    rxRing.clearPoisoned();
    return true;
}

size_t MorseBleSerial::txEnqueue(const uint8_t *buf, size_t len) {
    // protocol replies: try hard for bounded time (self-drain), then drop —
    // a stalled client must never stall the device. The tee feeds us one
    // write() per byte for serialized JSON, so the 20 ms budget must cover
    // the whole congestion episode, not each call: once the deadline passes,
    // txBackoff drops everything until pump() fully drains the ring.
    if (!linkUp())
        return len;
    size_t written = txRing.produceSome(buf, (uint16_t) len);
    if (written >= len)
        return len;
    if (txBackoff) {
        txDropped += (uint32_t)(len - written);
        return len;
    }
    uint32_t start = millis();
    for (;;) {
        written += txRing.produceSome(buf + written, (uint16_t)(len - written));
        if (written >= len)
            break;
        if ((uint32_t)(millis() - start) >= 20 || !linkUp()) {
            txDropped += (uint32_t)(len - written);         // torn JSON possible: client re-issues the GET
            txBackoff = true;
            break;
        }
        pump();
    }
    return len;
}

size_t MorseBleSerial::txEnqueueEcho(const uint8_t *buf, size_t len) {
    // echo channel sits in the keying path (SerialOutMorse): what fits, fits —
    // never drain, never wait, a dit at 40 WpM is only ~30 ms
    if (!linkUp())
        return len;
    txRing.produceSome(buf, (uint16_t) len);
    return len;
}

void MorseBleSerial::txFlush(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (isRunning && txRing.used() > 0 && (uint32_t)(millis() - start) < timeoutMs) {
        pump();
        if (!linkUp())
            break;
        delay(1);                       // let the BT task confirm notifications
    }
}

#endif /* #ifdef CONFIG_BLE_SERIAL */
