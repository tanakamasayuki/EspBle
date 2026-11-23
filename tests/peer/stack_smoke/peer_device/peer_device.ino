#include <Arduino.h>
#include <BLE.h>

#include "../../../sketch_support/EspBleTestLifecycle.h"

#if !defined(CONFIG_NIMBLE_ENABLED)
#error "EspBle peer tests require the Arduino-ESP32 NimBLE backend"
#endif

static const BLEUUID SERVICE_UUID("8d47a620-8d3a-4d65-a76f-6f7370626c65");
static const BLEUUID CHARACTERISTIC_UUID("8d47a621-8d3a-4d65-a76f-6f7370626c65");
static BLEServer server;

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!BLE.begin("EspBle Test Peer"))
  {
    Serial.println("PERIPHERAL_INIT_FAILED");
    return;
  }

  server = BLE.createServer();
  server.onConnect([](BLEServer &, const BLEConnInfo &) {
    Serial.println("PEER_CONNECTED");
  });
  BLEService service = server.createService(SERVICE_UUID);
  BLECharacteristic characteristic = service.createCharacteristic(
    CHARACTERISTIC_UUID,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermission::ReadWriteOpen);
  characteristic.onWrite([](BLECharacteristic &written, const BLEConnInfo &) {
    String value = written.getStringValue();
    Serial.printf("WRITE %s\n", value.c_str());
  });
  characteristic.setValue("peer-ready");
  if (!server.start())
  {
    Serial.println("PERIPHERAL_INIT_FAILED");
    return;
  }

  BLEAdvertising advertising = BLE.getAdvertising();
  advertising.addServiceUUID(SERVICE_UUID);
  advertising.setScanResponse(true);
  if (!BLE.startAdvertising())
  {
    Serial.println("PERIPHERAL_INIT_FAILED");
    return;
  }
  Serial.println("PERIPHERAL_READY");

  // The sketch tracks no connection id; the wrapper's own peer map has them.
  // This wrapper does not advertise again by itself once a link drops, so the
  // predicate brings advertising back. end() takes the radio down
  // synchronously, so STOP is complete at once.
  EspBleTestLifecycle::Hooks hooks;
  hooks.stop = []() {
    BLE.end(false);
  };
  EspBleTestLifecycle::begin(hooks);
}

void loop()
{
  if (Serial.available() > 0) EspBleTestLifecycle::handle(static_cast<char>(Serial.read()));
  EspBleTestLifecycle::update();
  delay(100);
}
