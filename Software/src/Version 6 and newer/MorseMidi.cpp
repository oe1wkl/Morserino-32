#ifdef CONFIG_BLE_MIDI

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

#include <Arduino.h>
#include "morsedefs.h"
#include "MorsePreferences.h"
#include "MorseMidi.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"

// Standard BLE MIDI service and characteristic UUIDs (MIDI Association spec)
#define MIDI_SERVICE_UUID        "03B80E5A-EDE8-4B33-A751-6CE34EC4C700"
#define MIDI_CHARACTERISTIC_UUID "7772E5DB-3868-4112-A1A9-F2669D106BF3"
#define MIDI_DEVICE_NAME         "Morserino32 MIDI"

bool MorseMidi::isMidiRunning  = false;
bool MorseMidi::isMidiConnected = false;

static TaskHandle_t midiTaskHandle = nullptr;
static BLECharacteristic* midiChar = nullptr;

class MidiServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        MorseMidi::isMidiConnected = true;
    }
    void onDisconnect(BLEServer* server) override {
        MorseMidi::isMidiConnected = false;
        delay(5000);
        server->startAdvertising();
    }
};

static void midiTask(void*) {
    BLEDevice::init(MIDI_DEVICE_NAME);
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new MidiServerCallbacks());

    BLEService* service = server->createService(MIDI_SERVICE_UUID);

    midiChar = service->createCharacteristic(
        MIDI_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ   |
        BLECharacteristic::PROPERTY_WRITE  |
        BLECharacteristic::PROPERTY_WRITE_NR |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    midiChar->addDescriptor(new BLE2902());

    service->start();

    BLEAdvertising* advertising = server->getAdvertising();
    advertising->addServiceUUID(MIDI_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->start();

    delay(portMAX_DELAY);
}

void MorseMidi::initializeMidi() {
    if (!isMidiRunning) {
        BaseType_t ret = xTaskCreate(midiTask, "ble_midi", 10000, nullptr, 3, &midiTaskHandle);
        if (ret == pdPASS)
            isMidiRunning = true;
        delay(100);
    }
}

void MorseMidi::stopMidi() {
    if (isMidiRunning) {
        if (midiTaskHandle) {
            vTaskDelete(midiTaskHandle);
            midiTaskHandle = nullptr;
        }
        delay(100);
        BLEDevice::deinit(true);
        delay(100);
        midiChar = nullptr;
        isMidiConnected = false;
        isMidiRunning = false;
    }
}

// Build a 5-byte BLE MIDI packet and send it via notification.
// Header/timestamp encoding follows the MIDI over BLE spec (13-bit ms timestamp).
static void sendMidiPacket(uint8_t status, uint8_t note, uint8_t velocity) {
    if (!MorseMidi::isMidiConnected || !midiChar)
        return;
    uint16_t ts = (uint16_t)(millis() & 0x1FFF);
    uint8_t header = 0x80 | (uint8_t)((ts >> 7) & 0x3F);
    uint8_t timestamp = 0x80 | (uint8_t)(ts & 0x7F);
    uint8_t packet[5] = { header, timestamp, status, note, velocity };
    midiChar->setValue(packet, sizeof(packet));
    midiChar->notify();
}

void MorseMidi::noteOn(uint8_t note) {
    uint8_t channel = (uint8_t)(MorsePreferences::pliste[posMidiChannel].value - 1) & 0x0F;
    sendMidiPacket(0x90 | channel, note & 0x7F, 100);
}

void MorseMidi::noteOff(uint8_t note) {
    uint8_t channel = (uint8_t)(MorsePreferences::pliste[posMidiChannel].value - 1) & 0x0F;
    sendMidiPacket(0x80 | channel, note & 0x7F, 0);
}

#endif // CONFIG_BLE_MIDI
