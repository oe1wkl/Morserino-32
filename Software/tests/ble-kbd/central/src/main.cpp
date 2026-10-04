// BLE keyboard test host for the Morserino's vBand keyboard soak (TODO I2).
//
// Plays the computer's part: scans for "Morserino32 Keyboard", connects, bonds (the M32's HID service
// requires an encrypted link), subscribes to the HID input report and prints, over USB at 115200:
//   READY | SCAN | FOUND <addr> | CONN <ms> | SUB <ms> | R <ms> <modifiers> <key0> | DROP <ms> |
//   DISC <ms> 0x<hci reason> | ERR <text>
// Commands (one per line): "drop" - disconnect now; "drop-on-down" - disconnect at the next report with
// Ctrl pressed (the moment a lost key-up would leave a real host keying); "auto 0|1" - reconnect
// automatically (default 1); "status".
// The callbacks run on Bluedroid's task, so they only queue events; loop() prints and acts.
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLESecurity.h>

static const char* TARGET = "Morserino32 Keyboard";

struct Ev { uint32_t t; char kind; uint8_t a; uint8_t b; };   // 'R' report (a = modifiers, b = key0), 'D' disconnect (a = reason),
                                                               // 'A' auth complete (a = success, b = fail reason), 'O' gattc open (a = status)
static QueueHandle_t q;
static BLEAddress* target = nullptr;
static esp_ble_addr_type_t targetType = BLE_ADDR_TYPE_PUBLIC;
static BLEClient* client = nullptr;
static volatile bool connected = false, authDone = false, authOk = false;
static volatile bool dropOnDown = false, dropNow = false;
static bool autoConnect = true;
static uint32_t reconnectDelay = 0;          // ms to wait after a disconnect before reconnecting ("delay <ms>")
static volatile uint32_t lastDisc = 0;

static void notifyCb(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
    Ev e{millis(), 'R', (uint8_t)(len > 0 ? data[0] : 0), (uint8_t)(len > 2 ? data[2] : 0)};
    xQueueSend(q, &e, 0);
    if (dropOnDown && len > 0 && (data[0] & 0x01)) { dropOnDown = false; dropNow = true; }
}

static void gattcHandler(esp_gattc_cb_event_t event, esp_gatt_if_t, esp_ble_gattc_cb_param_t* p) {
    if (event == ESP_GATTC_OPEN_EVT) {
        Ev e{millis(), 'O', (uint8_t)p->open.status, 0};
        xQueueSend(q, &e, 0);
    }
    if (event == ESP_GATTC_DISCONNECT_EVT) {
        Ev e{millis(), 'D', (uint8_t)p->disconnect.reason, 0};
        xQueueSend(q, &e, 0);
        lastDisc = millis();
        connected = false;
    }
}

class Sec : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() override { return 0; }
    void onPassKeyNotify(uint32_t) override {}
    bool onConfirmPIN(uint32_t) override { return true; }
    bool onSecurityRequest() override { return true; }
    void onAuthenticationComplete(esp_ble_auth_cmpl_t r) override {
        authOk = r.success; authDone = true;
        Ev e{millis(), 'A', (uint8_t)r.success, (uint8_t)r.fail_reason};
        xQueueSend(q, &e, 0);
    }
};

static bool findTarget() {
    Serial.println("SCAN");
    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    BLEScanResults res = scan->start(4, false);
    for (int i = 0; i < res.getCount(); ++i) {
        BLEAdvertisedDevice d = res.getDevice(i);
        if (d.haveName() && d.getName() == TARGET) {
            target = new BLEAddress(d.getAddress());
            targetType = d.getAddressType();
            Serial.printf("FOUND %s type %d rssi %d\n", target->toString().c_str(), (int)targetType, d.getRSSI());
            scan->clearResults();
            return true;
        }
    }
    scan->clearResults();
    return false;
}

static bool connectAndSubscribe() {
    authDone = authOk = false;
    if (!client->connect(*target, targetType)) {
        Serial.println("ERR connect");
        // Is the DUT advertising at all? (distinguishes "not advertising" from "advertising, not accepting")
        BLEScan* scan = BLEDevice::getScan();
        scan->setActiveScan(true);
        BLEScanResults res = scan->start(3, false);
        int rssi = 0; bool seen = false;
        for (int i = 0; i < res.getCount(); ++i) {
            BLEAdvertisedDevice d = res.getDevice(i);
            if (d.getAddress().equals(*target)) { seen = true; rssi = d.getRSSI(); }
        }
        scan->clearResults();
        if (seen) Serial.printf("SEEN %lu rssi %d\n", millis(), rssi);
        else      Serial.printf("NOTSEEN %lu\n", millis());
        return false;
    }
    connected = true;
    Serial.printf("CONN %lu\n", millis());
    uint32_t t0 = millis();
    while (!authDone && millis() - t0 < 4000) delay(10);          // bonding / encryption first
    if (!authOk) Serial.println("ERR auth (continuing)");
    BLERemoteService* hid = client->getService(BLEUUID((uint16_t)0x1812));
    if (!hid) { Serial.println("ERR no HID service"); client->disconnect(); return false; }
    // By handle: the input and the output report share UUID 0x2A4D, a by-UUID map keeps only one.
    for (auto& kv : *hid->getCharacteristicsByHandle()) {
        BLERemoteCharacteristic* c = kv.second;
        if (c->getUUID().equals(BLEUUID((uint16_t)0x2A4D)) && c->canNotify()) {
            c->registerForNotify(notifyCb);
            Serial.printf("SUB %lu\n", millis());
            return true;
        }
    }
    Serial.println("ERR no notifying input report");
    client->disconnect();
    return false;
}

void setup() {
    Serial.begin(115200);
    q = xQueueCreate(128, sizeof(Ev));
    BLEDevice::init("M32 kbd test host");
    BLEDevice::setCustomGattcHandler(gattcHandler);
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new Sec());
    BLESecurity* s = new BLESecurity();
    s->setAuthenticationMode(ESP_LE_AUTH_BOND);
    s->setCapability(ESP_IO_CAP_NONE);
    s->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    client = BLEDevice::createClient();
    Serial.println("READY");
}

void loop() {
    Ev e;
    while (xQueueReceive(q, &e, 0) == pdTRUE) {
        if (e.kind == 'R')      Serial.printf("R %lu %u %u\n", e.t, e.a, e.b);
        else if (e.kind == 'A') Serial.printf("AUTH %lu ok=%u reason=0x%02x\n", e.t, e.a, e.b);
        else if (e.kind == 'O') Serial.printf("OPEN %lu status=0x%02x\n", e.t, e.a);
        else                    Serial.printf("DISC %lu 0x%02x\n", e.t, e.a);
    }
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n'); cmd.trim();
        if (cmd == "drop") dropNow = true;
        else if (cmd == "drop-on-down") dropOnDown = true;
        else if (cmd == "auto 0") autoConnect = false;
        else if (cmd == "auto 1") autoConnect = true;
        else if (cmd.startsWith("delay ")) { reconnectDelay = cmd.substring(6).toInt(); Serial.printf("DELAY %lu\n", reconnectDelay); }
        else if (cmd == "status") Serial.printf("STATUS %lu connected=%d target=%s\n", millis(), (int)connected,
                                                target ? target->toString().c_str() : "-");
    }
    if (dropNow) {
        dropNow = false;
        if (connected) { Serial.printf("DROP %lu\n", millis()); client->disconnect(); }
    }
    if (!connected && autoConnect && millis() - lastDisc >= reconnectDelay) {
        if (!target && !findTarget()) { delay(200); return; }
        if (!connectAndSubscribe()) delay(300);
    }
    delay(1);
}
