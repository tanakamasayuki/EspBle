#include <EspBle.h>

static constexpr const char *TEST_SERVICE_UUID = "8d47a630-8d3a-4d65-a76f-6f7370626c65";

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Peer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  const uint8_t manufacturerData[] = {0x34, 0x12, 0xbe, 0xef};
  auto &advertising = ble.advertising();
  advertising.setName("EspBle Peer");
  advertising.addServiceUuid(TEST_SERVICE_UUID);
  advertising.setManufacturerData(manufacturerData, sizeof(manufacturerData));
  advertising.setScanResponseEnabled(true);
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == '?')
  {
    Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
  }

  ble.update();
  delay(1);
}
