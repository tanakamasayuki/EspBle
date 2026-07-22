// Eddystone-URL broadcaster for the eddystone peer test.
#include <EspBle.h>
#include <EspBleEddystone.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Eddystone";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  uint8_t body[EspBleEddystoneUrlMaxBodySize];
  size_t length = 0;
  if (!espBleEncodeEddystoneUrl("https://www.example.com/", -20, body, sizeof(body), length))
  {
    Serial.println("ENCODE_FAILED");
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setConnectable(false);
  advertising.setScanResponseEnabled(false);
  advertising.addServiceUuid(EspBleEddystoneServiceUuid);
  if (!advertising.setServiceData(EspBleEddystoneServiceUuid, body, length))
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
