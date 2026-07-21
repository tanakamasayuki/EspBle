// beacon peer_device: a non-connectable, non-scannable BLE beacon. It advertises
// a marker service UUID and manufacturer data with setConnectable(false) +
// setScanResponseEnabled(false) and a 100..150 ms advertising interval. No GATT
// connection is possible; the payload is broadcast only.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *MARKER_SERVICE_UUID = "1815";
// Company ID 0xFFFF (test/unassigned) followed by four payload bytes.
static const uint8_t manufacturerData[] = {0xFF, 0xFF, 0x01, 0x02, 0x03, 0x04};

EspBle ble;

void setup()
{
  Serial.begin(115200);
  delay(500);

  EspBleConfig config;
  config.deviceName = "EspBle Beacon";
  config.security.enabled = false;
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setConnectable(false);       // beacon: non-connectable
  advertising.setScanResponseEnabled(false); // non-scannable (pure broadcaster)
  advertising.addServiceUuid(MARKER_SERVICE_UUID);
  advertising.setManufacturerData(manufacturerData, sizeof(manufacturerData));
  if (!advertising.setInterval(100, 150))
  {
    Serial.printf("INTERVAL_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
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
