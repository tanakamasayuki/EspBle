// Advertiser for the scan_response peer test: split the payload across the
// advertising data (service UUID, appearance, Tx Power) and the scan response
// (name + manufacturer data), so a passive scanner sees only the former.
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "FEAC";
// Generic Thermometer; the value only has to be recognisable in the test.
static constexpr uint16_t APPEARANCE_THERMOMETER = 0x0341;

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Scan Response";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  // Advertising payload: the service UUID, appearance and Tx Power. No name
  // here, so a passive scanner cannot learn it, but it does see these.
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.data().setAppearance(APPEARANCE_THERMOMETER);
  advertising.data().setTxPowerIncluded(true);

  // Scan response payload: only an active scanner requests and receives this.
  const uint8_t manufacturerData[] = {0xFF, 0xFF, 0x51, 0x52};
  advertising.scanResponse().setName("EspBle Scan Response");
  advertising.scanResponse().setManufacturerData(manufacturerData, sizeof(manufacturerData));

  advertising.setConnectable(false);
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
