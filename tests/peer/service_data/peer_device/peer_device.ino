// Service Data broadcaster for the service_data peer test: advertise two Service
// Data blocks (AD type 0x16) under different 16-bit UUIDs.
#include <EspBle.h>

static constexpr const char *SERVICE_DATA_UUID = "FEAB";
static constexpr const char *SECOND_SERVICE_DATA_UUID = "181A";

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
  const uint8_t secondPayload[] = {0x2E, 0x09};

  auto &advertising = ble.advertising();
  advertising.setConnectable(false);
  advertising.setScanResponseEnabled(false);
  advertising.addServiceUuid(SERVICE_DATA_UUID);
  if (!advertising.addServiceData(SERVICE_DATA_UUID, payload, sizeof(payload)))
  {
    Serial.printf("SERVICE_DATA_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  if (!advertising.addServiceData(SECOND_SERVICE_DATA_UUID, secondPayload, sizeof(secondPayload)))
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
