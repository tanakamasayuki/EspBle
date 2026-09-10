#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include "../../../sketch_support/EspBleTestLifecycle.h"

#if !defined(CONFIG_NIMBLE_ENABLED)
#error "EspBle peer tests require the Arduino-ESP32 NimBLE backend"
#endif

static constexpr const char *SERVICE_UUID = "8d47a620-8d3a-4d65-a76f-6f7370626c65";
static constexpr const char *CHARACTERISTIC_UUID = "8d47a621-8d3a-4d65-a76f-6f7370626c65";
static BLEServer *server = nullptr;

class ServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *) override
  {
    Serial.println("PEER_CONNECTED");
  }
};

class CharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *characteristic) override
  {
    String value = characteristic->getValue();
    Serial.printf("WRITE %s\n", value.c_str());
  }
};

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!BLEDevice::init("EspBle Test Peer"))
  {
    Serial.println("PERIPHERAL_INIT_FAILED");
    return;
  }

  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService *service = server->createService(SERVICE_UUID);
  BLECharacteristic *characteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setCallbacks(new CharacteristicCallbacks());
  characteristic->setValue("peer-ready");
  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("PERIPHERAL_READY");

  // The sketch tracks no connection id; the wrapper's own peer map has them.
  // This wrapper does not advertise again by itself once a link drops, so the
  // predicate brings advertising back. deinit() takes the radio down
  // synchronously, so STOP is complete at once.
  EspBleTestLifecycle::Hooks hooks;
  hooks.stop = []() {
    BLEDevice::deinit(false);
  };
  EspBleTestLifecycle::begin(hooks);
}

void loop()
{
  if (Serial.available() > 0) EspBleTestLifecycle::handle(static_cast<char>(Serial.read()));
  EspBleTestLifecycle::update();
  delay(100);
}
