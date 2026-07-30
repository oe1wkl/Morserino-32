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
#include "MorsePreferences.h"    // pliste[posBluetoothOut]: are we selected on the Bluetooth Use selector?
#ifdef CONFIG_BLUETOOTH_KEYBOARD
#include "MorseBluetooth.h"      // isBLErunning: HID-owns-the-stack is a transient, not an init failure
#endif
#include "morsedefs.h"

#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLE2902.h"
#include <esp_bt.h>              // esp_bt_controller_get_status
#include <esp_bt_main.h>         // esp_bluedroid_get_status
#include <esp_gap_ble_api.h>     // direct advertising config (see init: the wrapper fails silently)
#include <esp_mac.h>             // esp_read_mac: derive our static random address

// Nordic UART Service: the de-facto BLE-UART standard — works out of the box
// with CoreBluetooth, bleak, nRF Connect. RX is written by the central (our
// command input), TX notifies the central (our protocol output).
static const char *NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *NUS_RX_UUID      = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static const char *NUS_TX_UUID      = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";
static const char BLE_NAME[]        = "Morserino-32";   // distinct from the HID's "Morserino32 Keyboard";
                                                        // carried in the scan response (no room next to the 128-bit UUID)
static_assert(sizeof(BLE_NAME) - 1 <= 29, "BLE_NAME must fit a 31-byte scan response (2 bytes AD header)");

volatile bool MorseBleSerial::isRunning = false;

static volatile bool isConnected = false;               // maintained by callbacks, cleared synchronously by stop();
                                                        // external code asks linkUp(), never this
static volatile bool sessionResetPending = false;
static volatile bool advertisePending = false;
static volatile bool lineResetLatch = false;            // pump() -> bleSerialEvent(): discard your partial line
static volatile uint16_t mtuPayload = 20;               // usable notify payload; 20 until the MTU event fires

static BleByteRing<1024> rxRing;                        // producer: onWrite (Bluedroid task); consumer: loop task
static BleByteRing<4096> txRing;                        // produced AND consumed on the loop task
static uint8_t staging[512];                            // contiguous chunk for notify()
static BleFlowGate flow;
static bool txBackoff = false;                          // TX ring overflowed: drop everything until pump() has fully
                                                        // drained it — txEnqueue never waits (it can sit in the CW
                                                        // keying path), so this is the whole congestion policy
static bool txBackoffLogPending = false;                // DEBUG deferred to pump(): txEnqueue runs BETWEEN the chunks
                                                        // of a JSON object streaming through the tee, and a DEBUG
                                                        // there splices raw text into the middle of the USB client's
                                                        // object (found on hardware: corrupted a concurrent USB GET)

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
        isConnected = true;                             // MTU is per-connection state — back to the ATT default
        sessionResetPending = true;                     // until this central's own MTU exchange (a stale large
    }                                                   // value would ATT-truncate notifies = silent byte loss)
    void onDisconnect(BLEServer *server) override {
        (void) server;
        isConnected = false;
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

// one advertising-start path for BOTH init() and pump()'s post-disconnect
// restart. The adv/scan-response payloads persist in the controller between
// stop/start; the parameters do NOT — a restart through the library default
// (BLEAdvertising::start) would advertise from the PUBLIC address again and
// any host bonded to the Bluetooth keyboard instantly re-captures the device.
static bool bleSerialStartAdvertising() {
    esp_ble_adv_params_t advParams = {};
    advParams.adv_int_min = 0x20;
    advParams.adv_int_max = 0x40;
    advParams.adv_type = ADV_TYPE_IND;
    advParams.own_addr_type = BLE_ADDR_TYPE_RANDOM;     // our own identity, distinct from the keyboard's
    advParams.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;
    advParams.channel_map = ADV_CHNL_ALL;
    advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    return esp_ble_gap_start_advertising(&advParams) == ESP_OK;
}

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
#ifdef CONFIG_BLUETOOTH_KEYBOARD
    // transient, not a failure: the HID keyboard currently owns the stack
    // (e.g. BLE Serial was enabled while in keyer mode — the keyboard started
    // first). Refuse WITHOUT the sticky latch and without a splash; the
    // backstop retries after menu_() stops the keyboard. Latching here would
    // disable BLE Serial until reboot (found on hardware, M32 Pocket).
    if (MorseBluetooth::isBLErunning)
        return false;
#endif
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
    // Deliberately heap-allocated per init(), like the library's own
    // server/service/characteristic objects (documented leak, a few dozen
    // bytes per suspend/resume cycle). A static instance does NOT work:
    // BLEDescriptor::executeCreate() refuses a descriptor whose m_handle is
    // already set (from the previous init cycle, setHandle() is private), so
    // the re-created characteristic would silently get NO CCCD — every
    // client's enable-notify then fails with ATT "attribute not found"
    // (found on hardware, M32 Pocket, first stop->init cycle).
    txChar->addDescriptor(new BLE2902());
    BLECharacteristic *rxChar = service->createCharacteristic(NUS_RX_UUID,
                                    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    rxChar->setCallbacks(&rxCallbacks);
    service->start();

    // flags (3 B) + 128-bit NUS UUID (18 B) fill the 31-byte ADV PDU; the
    // device name rides in the scan response so scanners still show it.
    // The scan response must be EXPLICIT (name-only): the library's
    // auto-built one (setScanResponse(true) alone) memcpy's the whole ADV
    // payload — 128-bit UUID included — beside the name, exceeds 31 bytes,
    // and esp_ble_gap_config_adv_data's INVALID_ARG makes start() bail
    // BEFORE esp_ble_gap_start_advertising: silently no advertising at all
    // (verified on hardware, M32 Pocket, core 2.0.17).
    // BLE Serial advertises from a STATIC RANDOM address, not the chip's
    // public one: hosts that bonded to the Bluetooth keyboard (VBand use)
    // auto-reconnect to the public identity the instant it advertises and
    // hold the connection — the device becomes invisible to every scanner
    // (found on hardware: this Mac's old keyboard pairing was the thief).
    // A distinct identity keeps old keyboard bonds and BLE Serial apart.
    uint8_t randAddr[6];
    esp_read_mac(randAddr, ESP_MAC_BT);
    randAddr[0] |= 0xC0;                                         // static-random address: two MSBs must be 11
    randAddr[5] ^= 0x55;                                         // and visibly distinct from the keyboard identity
    esp_ble_gap_set_rand_addr(randAddr);
    // advertising payloads via direct GAP calls: BLEAdvertising::start()'s
    // auto-built scan response would exceed 31 bytes and bail silently — see
    // the note above bleSerialStartAdvertising()
    static uint8_t nusUuidLE[16];
    memcpy(nusUuidLE, BLEUUID(NUS_SERVICE_UUID).to128().getNative()->uuid.uuid128, 16);
    esp_ble_adv_data_t advData = {};
    advData.set_scan_rsp = false;
    advData.include_name = false;
    advData.include_txpower = false;
    advData.appearance = 0;
    advData.flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;
    advData.service_uuid_len = 16;
    advData.p_service_uuid = nusUuidLE;
    esp_ble_gap_config_adv_data(&advData);
    static uint8_t scanRspRaw[2 + sizeof(BLE_NAME) - 1];
    scanRspRaw[0] = sizeof(BLE_NAME);                            // AD length: type byte + name (sizeof includes the NUL = +1 for the type)
    scanRspRaw[1] = 0x09;                                        // Complete Local Name
    memcpy(scanRspRaw + 2, BLE_NAME, sizeof(BLE_NAME) - 1);
    esp_ble_gap_config_scan_rsp_data_raw(scanRspRaw, sizeof(scanRspRaw));
    bleSerialStartAdvertising();

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
    txBackoffLogPending = false;
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
    if (txBackoffLogPending) {          // deferred from txEnqueue: pump() is only called between
        txBackoffLogPending = false;    // protocol lines, never mid-object in the tee
        DEBUG("BLE TX backoff: client not draining, dropping protocol bytes");
    }
    if (sessionResetPending) {
        sessionResetPending = false;    // clear FIRST (like takeLineReset): an edge the Bluedroid task
                                        // signals while this block runs must trigger another reset, not
                                        // be lost — stale lines from a dead session could execute
        rxRing.resetToMark();           // consumer-side only (PLAN D18): tail -> producer-latched mark
        rxRing.clearPoisoned();         // (latched at BOTH session edges), old-session bytes vanish, a
        txRing.hardReset();             // new central's early first write (before this pass) survives;
        flow.resetSession(millis());    // TX ring is loop-task-owned on both sides: trivially safe
        lineResetLatch = true;          // bleSerialEvent must drop its partial line
        bleProtocol = false;            // belt-and-braces: a handshake racing the disconnect edge must
                                        // not leave a dead session marked active (zombie sleep-suppression)
        txBackoff = false;
    }
    if (advertisePending && !isConnected && bleServer != nullptr) {
        bleSerialStartAdvertising();    // NEVER the library path: it would revert to the public address
        advertisePending = false;
    }
    if (!isConnected)                   // idle/advertising is the dominant state, and pump() runs at every
        return;                         // polling site — everything below only matters with a live central
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
    // the flag lifts the poison, so the producer admits writes again
    if (!rxRing.poisoned)
        return false;
    rxRing.clearPoisoned();
    return true;
}

size_t MorseBleSerial::txEnqueue(const uint8_t *buf, size_t len) {
    // protocol replies: enqueue what fits, NEVER wait — this can run inside
    // the CW keying path (speed/volume jsonControl from keyer mode), where
    // even a bounded stall is an audible timing error. pump(), called from
    // every polling site, does all the draining. On overflow we enter a
    // backoff episode and drop everything until pump() has fully drained the
    // ring (used()==0): the client sees at most one torn JSON object per
    // episode and re-issues the GET; checking txBackoff BEFORE producing
    // keeps fragments of later objects from splicing into the torn one.
    if (!linkUp())
        return len;
    if (txBackoff)
        return len;                     // congestion episode: drop until fully drained
    size_t written = txRing.produceSome(buf, (uint16_t) len);
    if (written < len) {
        txBackoff = true;               // torn JSON possible: client re-issues the GET
        txBackoffLogPending = true;     // report from pump(), never from inside the tee write path
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

void MorseBleSerial::stopWithNotice(const String &msg, uint32_t flushMs) {
    // the one graceful-shutdown ritual: tell the client (best-effort — the
    // message may arrive, anything after it will not), drain, tear down
    if (!isRunning) {
        stop();     // still reset ALL module state — in particular the sticky
        return;     // init-failure latch, so a pref off->on cycle can retry
    }
    if (protocolActive()) {
        M32TargetScope bleOnly(M32Target::BleOnly);     // OUR session's goodbye: a handshaken USB
        MorseJSON::jsonCreate("message", msg, "");      // client must not see it (session-scoped)
    }
    txFlush(flushMs);
    stop();
}

void MorseBleSerial::suspendForWifi() {
    // BLE and WiFi share the 2.4 GHz radio and were never co-resident in this
    // firmware; the top-menu backstop (MorseMenu wait loop) restarts us
    bool wasRunning = isRunning;
    stopWithNotice("BLE serial suspended: wireless mode", 500);
    if (wasRunning) {                   // splash once (UX_CONVENTIONS 10.1): a state change the user
                                        // must know about is shown on the device, not just in the
                                        // protocol message. wasRunning also keeps the defense-in-depth
                                        // second call (radio primitives) from splashing again.
        MorseOutput::printOnScroll(2, BOLD, 0, "BLE Ser. susp.");
        MorseOutput::refreshDisplay();
        delay(800);
    }
}

#endif /* #ifdef CONFIG_BLE_SERIAL */
