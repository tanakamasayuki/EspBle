// Service Data broadcaster for the service_data peer test: advertise an
// arbitrary Service Data block (AD type 0x16) under a 16-bit UUID.
#include <EspBle.h>

static constexpr const char *SERVICE_DATA_UUID = "FEAB";

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Service Data";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  const uint8_t payload[] = {0xAB, 0xCD, 0xEF, 0x12};

  auto &advertising = ble.advertising();
  advertising.setConnectable(false);
  advertising.setScanResponseEnabled(false);
  advertising.addServiceUuid(SERVICE_DATA_UUID);
  if (!advertising.setServiceData(SERVICE_DATA_UUID, payload, sizeof(payload)))
  {
    Serial.printf("SERVICE_DATA_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  advertising.setInterval(100, 150);
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
