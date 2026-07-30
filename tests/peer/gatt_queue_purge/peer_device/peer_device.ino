// gatt_queue_purge peer_device: a plain Peripheral with four readable
// characteristics, each with its own UUID so the central can tell which queued
// read a result belongs to. It does nothing but answer reads and report
// connect/disconnect.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "6b1d0000-9c4e-4a71-8f2d-3e5a7c9b1000";
static const char *CHARACTERISTIC_UUIDS[] = {
  "6b1d0001-9c4e-4a71-8f2d-3e5a7c9b1000",
  "6b1d0002-9c4e-4a71-8f2d-3e5a7c9b1000",
  "6b1d0003-9c4e-4a71-8f2d-3e5a7c9b1000",
  "6b1d0004-9c4e-4a71-8f2d-3e5a7c9b1000",
};
static constexpr size_t CharacteristicCount =
  sizeof(CHARACTERISTIC_UUIDS) / sizeof(CHARACTERISTIC_UUIDS[0]);

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  auto &server = ble.gattServer();
  const EspBleGattService service = server.addService(SERVICE_UUID);
  if (!service.valid())
  {
    Serial.printf("PROFILE_FAILED %s %s\n",
      ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  for (size_t index = 0; index < CharacteristicCount; ++index)
  {
    EspBleGattCharacteristicConfig config;
    config.readable = true;
    const EspBleGattCharacteristic characteristic =
      server.addCharacteristic(service, CHARACTERISTIC_UUIDS[index], config);
    if (!characteristic.valid())
    {
      Serial.printf("PROFILE_FAILED %s %s\n",
        ble.lastErrorName(), ble.lastErrorDetail().c_str());
      return;
    }
    server.setValue(characteristic, String("v") + String(index + 1));
  }

  EspBleConfig config;
  config.deviceName = "EspBle Queue Purge";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n",
      ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf("PERIPHERAL_DISCONNECTED id=%u\n", static_cast<unsigned>(connection.id));
    // Advertise again so the central can reconnect within the same test.
    ble.advertising().start();
  });
  ble.advertising().setName("EspBle Queue Purge");
  ble.advertising().addServiceUuid(SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("PERIPHERAL_READY advertising=%u chars=%u\n",
        ble.advertising().isAdvertising() ? 1 : 0,
        static_cast<unsigned>(CharacteristicCount));
    }
  }
  ble.update();
  delay(1);
}
