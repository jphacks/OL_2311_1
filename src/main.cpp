#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <M5Capsule.h>

#define BUFFER 64
#define sw_pin 5
const uint16_t receiverPin = 9;
const uint16_t transmitterPin = 4;

IRrecv irrecv(receiverPin);
IRsend irsend(transmitterPin);
decode_results results;
uint64_t receivedmyCode;
uint64_t receivedfdCode;

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
BLECharacteristic *pRxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
std::string rxValue = "000001";

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

int buttonState = 0;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer *pServer) {
        deviceConnected = false;
    }
};

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0) {
            Serial.print("Received Value: ");
            Serial.println(rxValue.c_str());
            receivedmyCode = strtoull(rxValue.c_str(), NULL, 16);
        }
    }
};

void setup() {
    Serial.begin(115200);

    BLEDevice::init("BLE UART");

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );

    pTxCharacteristic->addDescriptor(new BLE2902());

    pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );

    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("Waiting for a client connection to notify...");

    irrecv.enableIRIn();
    irsend.begin();
    auto cfg = M5.config();
    M5Capsule.begin(cfg);

    pinMode(sw_pin, INPUT_PULLUP);
}

void loop() {
    if (irrecv.decode(&results)) {
        // 赤外線信号を受信した場合
        if (results.value  < 100000000 && results.value != 0 && results.value != receivedmyCode) {
            Serial.print("Received IR code: 0x");
            Serial.println(results.value, HEX);
            
            // 受信した赤外線コードをBLE経由で送信
            char irCodeStr[17];
            snprintf(irCodeStr, 17, "%016llX", results.value);
            // 余分な0を削除
            int startIndex = 0;
            while (irCodeStr[startIndex] == '0' && startIndex < 15) {
                startIndex++;
            }
            pTxCharacteristic->setValue(irCodeStr + startIndex);
            pTxCharacteristic->notify();
            delay(1000);
        }
        irrecv.resume();
    }

    if (deviceConnected && receivedmyCode != 0) {
        int reading = digitalRead(sw_pin);
        if (!reading) {
            Serial.print("Re-sending IR code: 0x");
            Serial.println(receivedmyCode, HEX);
            irsend.sendNEC(receivedmyCode, 32);
            delay(1000);
        }
    }
}