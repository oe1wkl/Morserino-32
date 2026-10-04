#ifdef CONFIG_BLUETOOTH_KEYBOARD

/******************************************************************************************************************************
 *  morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
 *  Copyright (C) 2018-2025  Brian Mahaffy N6UGP                                                                            ***
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

/*
 * Portions derived from example by https://gist.github.com/manuelbl
*/

#include <Arduino.h>
#include "morsedefs.h"
#include "MorsePreferences.h"
#include "MorseBluetooth.h"
#include "MorseOutput.h"
#include "BLEDevice.h"
#include "BLE2902.h"
#include "BLE2904.h"
#include "BLEHIDDevice.h"     // only for the HID_KEYBOARD appearance constant: the table itself is built here
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"

#define DEVICE_NAME "Morserino32 Keyboard"
#define CTRL_KEY_MODIFIER	0x1
#define US_KEYBOARD

boolean MorseBluetooth::isBLErunning = false;

using namespace MorseBluetooth;

// Forward declarations
void bluetoothTask(void*);
void bluetoothTypeLCTRL(bool ctrl);
// static bool isBLErunning = false;

BaseType_t xReturned;

bool isBleConnected = false;

// set (on the loop task) while stopBluetooth() tears the stack down; read by
// onDisconnect (BLE event task) to skip the reconnect grace period + re-advertise
static volatile bool btStopping = false;

// Link bookkeeping (TODO I2: vBand link drops, possible stuck key). The BLE callbacks run on Bluedroid's
// event task and must not block it - until 2026-10 onDisconnect slept there for 5 s before re-advertising.
// They only record what happened; MorseBluetooth::tick() acts on it from the main loop, which is also the
// only task that sends input reports, so reports never race each other.
static BLEServer* bleServer = nullptr;
static volatile bool advertisePending = false;     // re-advertise once the disconnect has settled
static volatile uint32_t disconnectedAt = 0;
static volatile bool resyncPending = false;        // tell the host the key's real state after a (re)connect
static volatile uint32_t connectedAt = 0;
static bool ctrlDown = false;                      // what the keyer last asked for, connected or not
static uint16_t disconnectCount = 0;
// Advertising start results (GAP event), for the soak's "stops accepting connections" fault (TODO I2):
// 0xFF = nothing to report, else the esp_bt_status_t of the last advertising start that did not succeed.
static volatile uint8_t advStartFault = 0xFF;
// Link watchdog: a connection that is not encrypted within LINK_SETUP_MS is dropped. Every real HID host encrypts
// within about a second (bonded: the stored key; new: pairing), and the HID characteristics are unusable until it
// does. Found with the soak rig: now and then a central's stack never finished its side of a new connection, and
// the keyboard - having stopped advertising - waited for it forever (until CW Keyer was left).
static const uint32_t LINK_SETUP_MS = 10000;
static volatile bool linkEncrypted = false;
static volatile uint16_t connId = 0xFFFF;
// Diagnostics (DEBUG, i.e. on USB when Serial Output = Nothing): events the callbacks note for tick() to report,
// and a 30 s status line while the keyboard runs - for the "connected but silent" case seen with macOS.
static volatile bool connectUnreported = false;
static volatile int authResult = -1;                // -1 none, else (success << 8) | fail_reason, until reported
static uint32_t notifiesSent = 0;                   // input reports sent on the current connection
static uint32_t lastStatusAt = 0;
static volatile bool serviceChangePending = false;  // tell a bonded host to rediscover once encrypted
static volatile uint32_t encryptedAt = 0;

static void gapHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
    if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT && param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        advStartFault = (uint8_t) param->adv_start_cmpl.status;
    if (event == ESP_GAP_BLE_AUTH_CMPL_EVT) {
        if (param->ble_security.auth_cmpl.success) {
            linkEncrypted = true;
            serviceChangePending = true;
            encryptedAt = millis();
        }
        authResult = (param->ble_security.auth_cmpl.success ? 0x100 : 0) | param->ble_security.auth_cmpl.fail_reason;
    }
}

TaskHandle_t taskHandle;    //

// Message (report) sent when a key is pressed or released
struct InputReport {
    uint8_t modifiers;	     // bitmask: CTRL = 1, SHIFT = 2, ALT = 4
    uint8_t reserved;        // must be 0
    uint8_t pressedKeys[6];  // up to six concurrenlty pressed keys
};

// Message (report) received when an LED's state changed
struct OutputReport {
    uint8_t leds;            // bitmask: num lock = 1, caps lock = 2, scroll lock = 4, compose = 8, kana = 16
};


// The report map describes the HID device (a keyboard in this case) and
// the messages (reports in HID terms) sent and received.
static const uint8_t REPORT_MAP[] = {
    USAGE_PAGE(1),      0x01,       // Generic Desktop Controls
    USAGE(1),           0x06,       // Keyboard
    COLLECTION(1),      0x01,       // Application
    REPORT_ID(1),       0x01,       //   Report ID (1)
    USAGE_PAGE(1),      0x07,       //   Keyboard/Keypad
    USAGE_MINIMUM(1),   0xE0,       //   Keyboard Left Control
    USAGE_MAXIMUM(1),   0xE7,       //   Keyboard Right Control
    LOGICAL_MINIMUM(1), 0x00,       //   Each bit is either 0 or 1
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_COUNT(1),    0x08,       //   8 bits for the modifier keys
    REPORT_SIZE(1),     0x01,
    HIDINPUT(1),        0x02,       //   Data, Var, Abs
    REPORT_COUNT(1),    0x01,       //   1 byte (unused)
    REPORT_SIZE(1),     0x08,
    HIDINPUT(1),        0x01,       //   Const, Array, Abs
    REPORT_COUNT(1),    0x06,       //   6 bytes (for up to 6 concurrently pressed keys)
    REPORT_SIZE(1),     0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
    USAGE_MINIMUM(1),   0x00,
    USAGE_MAXIMUM(1),   0x65,
    HIDINPUT(1),        0x00,       //   Data, Array, Abs
    REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
    REPORT_SIZE(1),     0x01,
    USAGE_PAGE(1),      0x08,       //   LEDs
    USAGE_MINIMUM(1),   0x01,       //   Num Lock
    USAGE_MAXIMUM(1),   0x05,       //   Kana
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    HIDOUTPUT(1),       0x02,       //   Data, Var, Abs
    REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
    REPORT_SIZE(1),     0x03,
    HIDOUTPUT(1),       0x01,       //   Const, Array, Abs
    END_COLLECTION(0)               // End application collection
};

BLECharacteristic* input = nullptr;
BLECharacteristic* output = nullptr;

// The HID GATT table, built here rather than with the library's BLEHIDDevice so that every object is ours to
// free. The Arduino BLE library frees none of the GATT objects it hands out (no destructors that walk their
// children, and BLEDevice::deinit() only stops the stack), and ~BLEHIDDevice() is empty: each keyboard
// start/stop leaked the whole table - 13.2-13.7 KB per CW Keyer visit, measured on the Pocket 2026-10-04;
// after five visits the keyboard could no longer start (largest free block 8 KB). The same disease as BLE
// Serial's 3.7 KB per WiFi trip (00ee378), same cure: free them after deinit(false), children first.
// The layout mirrors BLEHIDDevice (Arduino-ESP32 2.0.17): device info + HID + battery services.
//
// The attribute layout must not change. The library creates a service's characteristics, and a characteristic's
// descriptors, in the order of their heap ADDRESSES (BLEService/BLECharacteristic keep them in std::maps keyed
// by pointer), not in the order they were added; a bonded host caches the layout and never asks again. When the
// table was built from separate news, the input report's CCCD and report reference swapped handles relative to
// what a Mac had cached under master: the Mac wrote "notify on" (01 00) into the report reference, later read it
// back as report type 0 and stopped subscribing to the input report at all - a connected, encrypted keyboard that
// macOS silently ignores (I2, 2026-10-04). So the whole table is ONE object: members lie at ascending addresses
// in declaration order, which is BLEHIDDevice's order (and what master produced): manufacturer, PnP; HID info,
// report map, control point, protocol mode, input (reference, CCCD), output (reference); battery (format, CCCD).
// Report references are read-only, as the HID spec has them. Hosts that cached another layout are told to
// rediscover by a Service Changed indication once the link is encrypted (see gattsHandler / tick).
struct HidTable {
    BLECharacteristic manufacturer { BLEUUID((uint16_t) 0x2a29), BLECharacteristic::PROPERTY_READ };
    BLECharacteristic pnp          { BLEUUID((uint16_t) 0x2a50), BLECharacteristic::PROPERTY_READ };
    BLECharacteristic hidInfo      { BLEUUID((uint16_t) 0x2a4a), BLECharacteristic::PROPERTY_READ };
    BLECharacteristic reportMap    { BLEUUID((uint16_t) 0x2a4b), BLECharacteristic::PROPERTY_READ };
    BLECharacteristic hidControl   { BLEUUID((uint16_t) 0x2a4c), BLECharacteristic::PROPERTY_WRITE_NR };
    BLECharacteristic protocolMode { BLEUUID((uint16_t) 0x2a4e), BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_READ };
    BLECharacteristic input        { BLEUUID((uint16_t) 0x2a4d), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY };
    BLEDescriptor     inputRef     { BLEUUID((uint16_t) 0x2908) };
    BLE2902           inputCccd;
    BLECharacteristic output       { BLEUUID((uint16_t) 0x2a4d),
                                     BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR };
    BLEDescriptor     outputRef    { BLEUUID((uint16_t) 0x2908) };
    BLE2904           batteryFormat;
    BLECharacteristic batteryLevel { BLEUUID((uint16_t) 0x2a19), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY };
    BLE2902           batteryCccd;
};
static HidTable* hidTable = nullptr;
static BLEService *svcDeviceInfo, *svcHid, *svcBattery;
static BLEDescriptor* dInputCccd = nullptr;     // for the status line

// Telling a bonded host that cached another layout to look again. The Arduino core's Bluedroid is built with
// Service Changed in AUTO mode, which refuses esp_ble_gatts_send_service_change_indication() ("can't send
// service change indication manually"); in that mode it indicates Service Changed to every connected peer when
// a new service is started. So once per Bluetooth start, 2 s after the first encryption, an empty marker service
// is added: a real database change, so a host that holds a stale map - from master, whose order was heap luck,
// or from this branch's first table - rediscovers and resubscribes instead of ignoring the keyboard for good.
static const char* MARKER_SERVICE_UUID = "4d33322d-6c61-796f-7574-2d65706f6368";   // "M32-layout-epoch"
static BLEService* svcMarker = nullptr;

static void buildHidTable(BLEServer* server) {
    HidTable* t = hidTable = new HidTable();
    svcDeviceInfo = server->createService(BLEUUID((uint16_t) 0x180a));
    svcHid        = server->createService(BLEUUID((uint16_t) 0x1812), 40);
    svcBattery    = server->createService(BLEUUID((uint16_t) 0x180f));

    t->manufacturer.setValue("Morserino32");
    const uint8_t pnp[] = { 0x02, 0xe5, 0x02, 0xa1, 0x11, 0x02, 0x10 };   // USB VID 0xe502, PID 0xa111, version 0x0210
    t->pnp.setValue((uint8_t*) pnp, sizeof(pnp));
    svcDeviceInfo->addCharacteristic(&t->manufacturer);
    svcDeviceInfo->addCharacteristic(&t->pnp);

    const uint8_t info[] = { 0x11, 0x01, 0x00, 0x02 };                 // HID 1.11, not localized, normally connectable
    t->hidInfo.setValue((uint8_t*) info, sizeof(info));
    t->reportMap.setValue((uint8_t*) REPORT_MAP, sizeof(REPORT_MAP));
    const uint8_t reportMode[] = { 0x01 };
    t->protocolMode.setValue((uint8_t*) reportMode, 1);

    // input report 1: notify, encrypted, with report reference {id 1, input} and CCCD
    t->input.setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    t->inputRef.setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    const uint8_t inRef[] = { 1, 0x01 };
    t->inputRef.setValue((uint8_t*) inRef, 2);
    t->inputCccd.setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    t->input.addDescriptor(&t->inputRef);
    t->input.addDescriptor(&t->inputCccd);

    // output report 1 (LEDs): encrypted, with report reference {id 1, output}
    t->output.setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
    t->outputRef.setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
    const uint8_t outRef[] = { 1, 0x02 };
    t->outputRef.setValue((uint8_t*) outRef, 2);
    t->output.addDescriptor(&t->outputRef);

    for (BLECharacteristic* c : { &t->hidInfo, &t->reportMap, &t->hidControl, &t->protocolMode, &t->input, &t->output })
        svcHid->addCharacteristic(c);

    // battery level, with presentation format and CCCD (notifications on by default, as the library does)
    t->batteryFormat.setFormat(BLE2904::FORMAT_UINT8);
    t->batteryFormat.setNamespace(1);
    t->batteryFormat.setUnit(0x27ad);
    t->batteryCccd.setNotifications(true);
    t->batteryLevel.addDescriptor(&t->batteryFormat);
    t->batteryLevel.addDescriptor(&t->batteryCccd);
    svcBattery->addCharacteristic(&t->batteryLevel);

    input = &t->input;
    output = &t->output;
    dInputCccd = &t->inputCccd;

    svcDeviceInfo->start();
    svcHid->start();
    svcBattery->start();
}

// Only after BLEDevice::deinit(): with Bluedroid down no event can reach these objects, and their destructors
// merely free memory (semaphores, strings, containers). BLEDevice keeps a stale m_pServer, read only by its
// GATTS handler (dead until the next init) and overwritten by the next createServer() - as for BLE Serial.
static void freeHidTable() {
    delete hidTable;
    delete svcDeviceInfo; delete svcHid; delete svcBattery; delete svcMarker;
    delete bleServer;
    hidTable = nullptr;
    input = output = nullptr;
    dInputCccd = nullptr;
    svcDeviceInfo = svcHid = svcBattery = svcMarker = nullptr;
    bleServer = nullptr;
}

const InputReport NO_KEY_PRESSED = { };

/*
 * Callbacks related to BLE connection
 */
class BleKeyboardCallbacks : public BLEServerCallbacks {

    void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) {
        isBleConnected = true;
        connectedAt = millis();
        resyncPending = true;
        linkEncrypted = false;
        connId = param ? param->connect.conn_id : 0xFFFF;
        notifiesSent = 0;
        connectUnreported = true;

        // Allow notifications for characteristics
        BLE2902* cccDesc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        cccDesc->setNotifications(true);

        //DEBUG("Client has connected");
    }

    // The overload with the GATT parameters, for the disconnect reason (the library calls both overloads).
    void onDisconnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) {
        isBleConnected = false;
        resyncPending = false;
        ++disconnectCount;
        // Reported by tick() from the main loop. HCI reasons: 0x08 supervision timeout (the link went
        // silent - radio, or one side stalled), 0x13 the host hung up, 0x16 we did, 0x3e never established.
        lastReason = param ? param->disconnect.reason : 0xFF;
        reasonUnreported = true;

        // Teardown in progress (stopBluetooth): deinit() waits behind this callback,
        // and re-advertising a dying stack makes no sense. Just get out of the way.
        if (btStopping)
            return;

        // Disallow notifications for characteristics
        BLE2902* cccDesc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
        cccDesc->setNotifications(false);

        disconnectedAt = millis();
        advertisePending = true;                    // tick() restarts advertising - never sleep here
    }

public:
    volatile uint8_t lastReason = 0;
    volatile bool reasonUnreported = false;
};

// File scope (was local to bluetoothTask): tick() reads the disconnect reason from it.
static BleKeyboardCallbacks keyboardCallbacks;


/*
 * Called when the client (computer, smart phone) wants to turn on or off
 * the LEDs in the keyboard.
 *
 * bit 0 - NUM LOCK
 * bit 1 - CAPS LOCK
 * bit 2 - SCROLL LOCK
 */
class OutputCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) {
        OutputReport* report = (OutputReport*) characteristic->getData();
    }
};

void bluetoothTask(void*) {

    // This task may run once per keyer session (stopBluetooth on every menu
    // return, restart on the next keyer entry). The callback objects are static;
    // the GATT table is rebuilt each time and freed by stopBluetooth() (see
    // buildHidTable / freeHidTable).
    static OutputCallbacks outputCallbacks;
    static bool advertisingConfigured = false;
    static BLESecurity security;

    // initialize the device
    BLEDevice::init(DEVICE_NAME);
    BLEDevice::setCustomGapHandler(gapHandler);
    BLEServer* server = BLEDevice::createServer();
    bleServer = server;
    server->setCallbacks(&keyboardCallbacks);

    // Security: device requires bonding
    security.setAuthenticationMode(ESP_LE_AUTH_BOND);

    buildHidTable(server);
    output->setCallbacks(&outputCallbacks);

    // advertise the services. The BLEAdvertising object outlives deinit() (BLEDevice keeps it) and
    // addServiceUUID() only appends, so the UUIDs are added once per boot: until 2026-10 every keyboard start
    // added all three again, and the growing list pushed the advertising data past its 31 bytes.
    BLEAdvertising* advertising = server->getAdvertising();
    if (!advertisingConfigured) {
        advertising->setAppearance(HID_KEYBOARD);
        advertising->addServiceUUID(BLEUUID((uint16_t) 0x1812));     // HID
        advertising->addServiceUUID(BLEUUID((uint16_t) 0x180a));     // device information
        advertising->addServiceUUID(BLEUUID((uint16_t) 0x180f));     // battery
        advertisingConfigured = true;
    }
    advertising->start();

    // DEBUG("BLE ready");
    delay(portMAX_DELAY);
};

uint8_t MorseBluetooth::keyboardMode(void)
{
#ifdef CONFIG_BLE_SERIAL
    if (MorsePreferences::pliste[posBluetoothOut].value == BLT_USE_SERIAL_PROT)
        return 0;                       // selector gives BLE to the serial protocol: no keyboard mode
#endif
    return MorsePreferences::pliste[posBluetoothOut].value;
}

void MorseBluetooth::initializeBluetooth(void)
{
#ifdef CONFIG_BLE_SERIAL
    // the "Bluetooth Use" selector gives the BLE stack to the serial
    // protocol at value 5 — never start the keyboard's GATT server then;
    // enforced here so every present and future call site is safe
    if (MorsePreferences::pliste[posBluetoothOut].value == BLT_USE_SERIAL_PROT)
        return;
#endif
	// start Bluetooth task - stack size was originally 20000m prio was 5
    if (!isBLErunning) {
        DEBUG("BLE kbd: start, heap " + String(ESP.getFreeHeap()) + " (min " + String(ESP.getMinFreeHeap()) + ")");
	    xReturned = xTaskCreate(bluetoothTask, "bluetooth", 10000, NULL, 3, &taskHandle);
        if (xReturned == pdPASS)
            isBLErunning = true;
    delay(100);
    //DEBUG("BLE running?: " + String(isBLErunning));
    }
}


// Called from the main loop while the keyboard runs: everything the BLE callbacks may not do themselves.
bool MorseBluetooth::hostConnected(void)
{
    return isBLErunning && isBleConnected;
}

static bool logoShown = false;                      // the Bluetooth symbol as last drawn by tick()

void MorseBluetooth::tick(void)
{
    if (!isBLErunning)
        return;
    if (isBleConnected != logoShown) {              // show whether a host is connected; CW Keyer's top-line
        logoShown = isBleConnected;                 // repaint (updateTopLine) draws it too
        if (logoShown)
            MorseOutput::dispBleLogo();
        else
            MorseOutput::clearBleLogo();
    }
    if (keyboardCallbacks.reasonUnreported) {
        keyboardCallbacks.reasonUnreported = false;
        DEBUG("BLE kbd: link lost, reason 0x" + String(keyboardCallbacks.lastReason, HEX)
              + " (" + String(disconnectCount) + " since start, up " + String(millis() / 60000) + " min, "
              + String(bleServer ? bleServer->getConnectedCount() : 0) + " still connected, heap "
              + String(ESP.getFreeHeap()) + ")");
    }
    if (connectUnreported) {
        connectUnreported = false;
        DEBUG("BLE kbd: connected, conn " + String(connId));
        auto h = [](uint16_t v) { return String(v, HEX); };
        HidTable* t = hidTable;
        DEBUG("BLE kbd: handles manuf " + h(t->manufacturer.getHandle()) + " pnp " + h(t->pnp.getHandle())
              + " info " + h(t->hidInfo.getHandle()) + " map " + h(t->reportMap.getHandle()) + " ctrl " + h(t->hidControl.getHandle())
              + " proto " + h(t->protocolMode.getHandle()) + " in " + h(t->input.getHandle()) + " in-ref " + h(t->inputRef.getHandle())
              + " in-cccd " + h(t->inputCccd.getHandle()) + " out " + h(t->output.getHandle()) + " out-ref " + h(t->outputRef.getHandle())
              + " batt " + h(t->batteryLevel.getHandle()));
    }
    if (authResult >= 0) {
        int a = authResult;
        authResult = -1;
        DEBUG(String("BLE kbd: encryption ") + ((a & 0x100) ? "ok" : "FAILED") + ", reason 0x" + String(a & 0xFF, HEX));
    }
    if (serviceChangePending && millis() - encryptedAt >= 2000) {
        serviceChangePending = false;
        if (isBleConnected && bleServer && !svcMarker) {
            svcMarker = bleServer->createService(BLEUUID(MARKER_SERVICE_UUID), 2);
            svcMarker->start();
            DEBUG("BLE kbd: marker service started (Service Changed to the host)");
        }
    }
    if (millis() - lastStatusAt >= 30000) {
        lastStatusAt = millis();
        DEBUG(String("BLE kbd: status ") + (isBleConnected ? "connected" : "not connected")
              + (isBleConnected ? String(linkEncrypted ? ", encrypted" : ", NOT encrypted")
                                  + ", cccd " + String(dInputCccd ? (int) ((BLE2902*) dInputCccd)->getNotifications() : -1)
                                  + ", reports " + String(notifiesSent) : String(""))
              + ", heap " + String(ESP.getFreeHeap()));
    }
    if (advStartFault != 0xFF) {
        DEBUG("BLE kbd: advertising start FAILED, status 0x" + String(advStartFault, HEX));
        advStartFault = 0xFF;
    }
    if (advertisePending && millis() - disconnectedAt >= 500 && bleServer) {
        advertisePending = false;
        bleServer->startAdvertising();
    }
    // A link that is still not encrypted after LINK_SETUP_MS will never be usable: drop it, so that
    // onDisconnect -> advertising lets the host (or another one) connect again.
    if (isBleConnected && !linkEncrypted && connId != 0xFFFF && millis() - connectedAt >= LINK_SETUP_MS) {
        DEBUG("BLE kbd: link not encrypted after " + String(LINK_SETUP_MS / 1000) + " s - dropping it");
        uint16_t id = connId;
        connId = 0xFFFF;                    // once
        bleServer->disconnect(id);
    }
    // After a (re)connect, send the key's real state once the host has subscribed: a key-up lost in a
    // dropout (or a disconnect between press and release) would otherwise leave vBand keying until
    // the next element. A second is ample for subscription + encryption on a reconnect.
    if (resyncPending && isBleConnected && millis() - connectedAt >= 1000) {
        resyncPending = false;
        bluetoothTypeLCTRL(ctrlDown);
    }
}

void MorseBluetooth::stopBluetooth(void)
{
    if (MorseBluetooth::isBLErunning) {
        DEBUG("Stopping BLE");
        if (isBleConnected && input) {      // never leave the host with the key down (e.g. key held on exit)
            input->setValue((uint8_t *)&NO_KEY_PRESSED, sizeof(NO_KEY_PRESSED));
            input->notify();
            delay(60);                      // a few connection intervals for it to go out before teardown
        }
        ctrlDown = false;
        advertisePending = false;
        resyncPending = false;
        connId = 0xFFFF;
        btStopping = true;          // tell onDisconnect to skip re-advertising
        vTaskDelete(taskHandle);
        delay(100);
        // deinit(true) releases the BT controller memory irreversibly AND leaves the
        // library's 'initialized' flag set (BLEDevice.cpp, core 2.0.17): any later
        // init() is then a silent no-op and createServer() blocks forever in
        // registerApp — the keyboard could never restart until reboot (every
        // keyer -> menu -> keyer cycle). deinit(false) keeps the stack
        // re-initializable; in exchange the controller BSS stays reserved
        // for the rest of the boot. (BLE Serial also relies on this: it must be
        // able to (re)init the stack after a keyboard session — see
        // devdocs/ble-serial/DESIGN.md.)
        BLEDevice::deinit(false);
        freeHidTable();             // after deinit, never before
        delay(100);
        MorseBluetooth::isBLErunning = false;
        isBleConnected = false;     // onDisconnect is not delivered through deinit
        logoShown = false;          // the menu repaints the top line (and clears the symbol)
        DEBUG("BLE kbd: stopped, heap " + String(ESP.getFreeHeap()));
        btStopping = false;
    }
}

void MorseBluetooth::bluetoothTypeLCTRL(bool ctrl)
{
	ctrlDown = ctrl;                // tracked while disconnected too: tick() re-syncs the host on connect
	if (isBleConnected) {
		if (ctrl)
		{ // Send Left CTRL key pressed
			// create input report
			InputReport report = {
				.modifiers = CTRL_KEY_MODIFIER,
				.reserved = 0,
				.pressedKeys = {
					0, // key[0]
					0, 0, 0, 0, 0}}; // No Keys pressed.

			// send the input report
			input->setValue((uint8_t *)&report, sizeof(report));
			input->notify();
			++notifiesSent;
		}
		else
		{ // Send no key pressed
			// release all keys between two characters; otherwise two identical
			// consecutive characters are treated as just one key press
			input->setValue((uint8_t *)&NO_KEY_PRESSED, sizeof(NO_KEY_PRESSED));
			input->notify();
			++notifiesSent;
		}
	}
}

void MorseBluetooth::bluetoothTypeCharacter(const char chr)
{
	if (isBleConnected)
	{
		// translate character to key combination
		uint8_t val = (uint8_t)chr;
		if (val >= KEYMAP_SIZE)
			return; // character not available on keyboard - skip
		KEYMAP map = keymap[val];   // not keymap[chr]: char is signed, a UTF-8 byte would index below the table

		// create input report
		InputReport report = {
			.modifiers = map.modifier,
			.reserved = 0,
			.pressedKeys = {
				map.usage,
				0, 0, 0, 0, 0}};

		// send the input report
		input->setValue((uint8_t *)&report, sizeof(report));
		input->notify();

		delay(5);

		// release all keys between characters; otherwise two identical
		// consecutive characters are treated as just one key press
		input->setValue((uint8_t *)&NO_KEY_PRESSED, sizeof(NO_KEY_PRESSED));
		input->notify();
	}
}

// Emit a Shift+Enter HID report (soft return in word processors / chat apps).
// Reuses the active keymap's Return entry so we honour whatever HID layout
// the build was compiled with, just OR-ing in Left Shift.
static void bluetoothTypeShiftReturn(void)
{
    if (isBleConnected) {
        KEYMAP map = keymap[(uint8_t)'\n'];
        InputReport report = {
            .modifiers = (uint8_t)(map.modifier | 0x02),  // Left Shift
            .reserved = 0,
            .pressedKeys = { map.usage, 0, 0, 0, 0, 0 }
        };
        input->setValue((uint8_t*)&report, sizeof(report));
        input->notify();
        delay(5);
        input->setValue((uint8_t*)&NO_KEY_PRESSED, sizeof(NO_KEY_PRESSED));
        input->notify();
    }
}

// A HID keyboard sends key positions, not characters, and which character a position types depends
// on the host's keyboard layout - so the decoder's national letters (ä ö ü, and the Decoder Chars
// ones) are typed transliterated: å -> aa, æ -> ae, ø -> oe, é -> e, ... Argument: the second byte
// of a UTF-8 0xC3 pair. Anything else types nothing.
static const char* asciiForLatin1(uint8_t b) {
    switch (b) {
        case 0xA4: case 0xA6: return "ae";   case 0x84: case 0x86: return "AE";   // ä æ  Ä Æ
        case 0xB6: case 0xB8: return "oe";   case 0x96: case 0x98: return "OE";   // ö ø  Ö Ø
        case 0xBC:            return "ue";   case 0x9C:            return "UE";   // ü    Ü
        case 0xA5:            return "aa";   case 0x85:            return "AA";   // å    Å
        case 0xA0:            return "a";    case 0x80:            return "A";    // à    À
        case 0xA8: case 0xA9: return "e";    case 0x88: case 0x89: return "E";    // è é  È É
        case 0xA7:            return "c";    case 0x87:            return "C";    // ç    Ç
        case 0xB1:            return "n";    case 0x91:            return "N";    // ñ    Ñ
        case 0x9F:            return "ss";                                        // ß
        default:              return "";
    }
}

void MorseBluetooth::bluetoothTypeString(const String& str) {
    // Build a local modified copy only when needed
    String modified;
    const String* toSend = &str;

    if (keyboardMode() >= 0x4) {
        modified = str;
        modified.replace("<ERR>", "\b");
        modified.replace("<err>", "\b");
        modified.replace("<KA>", "\n");
        modified.replace("<ka>", "\n");
        if (MorsePreferences::pliste[posBluetoothARkey].value) {
            // \v is a sentinel: nothing else in the morse stream produces it,
            // and the main loop below intercepts it before bluetoothTypeCharacter.
            // The decoder emits '+' for .-.-. (same code as <AR>), so the
            // '+' replace is the one that actually fires in practice; the
            // <AR>/<ar> entries are belt-and-braces for any path that might
            // emit them literally.
            modified.replace("+", "\v");
            modified.replace("<AR>", "\v");
            modified.replace("<ar>", "\v");
        }
        // '+' default behaviour needs no replacement — the keymap already
        // types '+' from the literal character the decoder emits.
        toSend = &modified;
    }
    for (int i = 0; i < toSend->length(); i++) {
        char c = (*toSend)[i];
        if (c == '\v')
            bluetoothTypeShiftReturn();
        else if ((uint8_t) c == 0xC3 && i + 1 < toSend->length()) {     // UTF-8 letter: type it in ASCII
            for (const char* a = asciiForLatin1((uint8_t) (*toSend)[++i]); *a; ++a)
                bluetoothTypeCharacter(*a);
        }
        else
            bluetoothTypeCharacter(c);
    }
}
#endif //#ifdef CONFIG_BLUETOOTH_KEYBOARD