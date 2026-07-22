// Peripheral for the address_privacy test: advertise with a random static
// address instead of the factory public address.
#include <EspBle.h>

static constexpr const char *MARKER_SERVICE_UUID = "70726976-6163-7900-9003-72616e646d01";

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Privacy Peer";
  // Present a random static address (privacy: hides the public address).
  config.ownAddressType = EspBleOwnAddressType::RandomStatic;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Privacy Peer");
  advertising.addServiceUuid(MARKER_SERVICE_UUID);
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
