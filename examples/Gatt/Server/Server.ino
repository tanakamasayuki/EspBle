#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "10da4dd0-8eaa-4c69-9003-676174747277";
static constexpr const char *CHARACTERISTIC_UUID = "10da4dd1-8eaa-4c69-9003-676174747277";

EspBle ble;

void setup()
{
  Serial.begin(115200);

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;
  valueConfig.writable = true;

  if (!gattServer.addService(SERVICE_UUID) ||
      !gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, valueConfig) ||
      !gattServer.setValue(SERVICE_UUID, CHARACTERISTIC_UUID, String("ready")))
  {
    Serial.printf("GATT configuration failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  gattServer.onWritten([](const EspBleGattWrite &write) {
    Serial.printf(
      "Connection %u wrote: %s\n",
      static_cast<unsigned>(write.connectionId),
      write.value.c_str());
  });

  EspBleConfig config;
  config.deviceName = "EspBle GATT Server";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setName("EspBle GATT Server");
  advertising.addServiceUuid(SERVICE_UUID);
  if (!advertising.start())
  {
    Serial.printf("Advertising failed: %s\n", ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  ble.update();
  delay(1);
}
