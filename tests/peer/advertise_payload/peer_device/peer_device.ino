#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "BlePayload";
  if (!ble.begin(config))
  {
    Serial.printf("DEVICE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("BlePayload");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("DEVICE_ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'a')
    {
      // Two 16-bit service UUIDs: the CSS requires them to share a single
      // "Complete List of 16-bit Service UUIDs" AD structure.
      ble.advertising().clear();
      ble.advertising().setName("BlePayload");
      ble.advertising().setScanResponseEnabled(false);
      const bool added = ble.advertising().addServiceUuid("1812") &&
        ble.advertising().addServiceUuid("180f");
      Serial.printf("DEVICE_ADVERTISE_STARTED success=%u\n",
        (added && ble.advertising().start()) ? 1 : 0);
    }
  }

  ble.update();
  delay(1);
}
