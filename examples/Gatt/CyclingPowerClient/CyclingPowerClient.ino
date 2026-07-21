// en: CyclingPowerClient - connect to a Cycling Power Service (0x1818), read
//     Sensor Location, subscribe to Cycling Power Measurement notifications, and
//     decode the 16-bit flags and signed 16-bit instantaneous power.
// ja: CyclingPowerClient - Cycling Power Service（0x1818）へ接続し、Sensor
//     LocationをRead、Cycling Power MeasurementのNotificationを購読して、16bit
//     flagsと符号付き16bit instantaneous powerをデコードする。
#include <EspBle.h>

static constexpr const char *CYCLING_POWER_SERVICE_UUID = "1818";
static constexpr const char *CYCLING_POWER_MEASUREMENT_UUID = "2a63";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;

void setup()
{
  Serial.begin(115200);
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    ble.readCharacteristic(connection.id, CYCLING_POWER_SERVICE_UUID, SENSOR_LOCATION_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 1)
    {
      Serial.printf("Sensor location: %u\n", static_cast<uint8_t>(result.value[0]));
      ble.subscribe(result.connectionId, CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(CYCLING_POWER_MEASUREMENT_UUID) ||
        notification.value.length() < 4)
      return;
    const String &value = notification.value;
    const uint16_t rawPower = static_cast<uint8_t>(value[2]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(value[3])) << 8);
    // Instantaneous power is a signed 16-bit value in watts.
    Serial.printf("Power: %d W\n", static_cast<int16_t>(rawPower));
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(CYCLING_POWER_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  ble.update();
  delay(1);
}
