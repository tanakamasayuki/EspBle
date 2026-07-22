// iBeacon broadcaster for the ibeacon peer test.
#include <EspBle.h>
#include <EspBleIBeacon.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle iBeacon";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  EspBleIBeaconData beacon;
  const uint8_t uuid[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
  memcpy(beacon.uuid, uuid, 16);
  beacon.major = 0x1234;
  beacon.minor = 0xABCD;
  beacon.measuredPower = -59;

  uint8_t payload[EspBleIBeaconManufacturerDataSize];
  espBleEncodeIBeacon(beacon, payload);

  auto &advertising = ble.advertising();
  advertising.setConnectable(false);
  advertising.setScanResponseEnabled(false);
  advertising.setManufacturerData(payload, sizeof(payload));
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
