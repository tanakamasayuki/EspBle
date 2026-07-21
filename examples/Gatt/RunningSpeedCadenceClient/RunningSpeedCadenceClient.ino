// en: RunningSpeedCadenceClient - connect to a Running Speed and Cadence
//     Service (0x1814), read Sensor Location, subscribe to RSC Measurement
//     notifications, and decode speed/cadence/stride/distance.
// ja: RunningSpeedCadenceClient - Running Speed and Cadence Service（0x1814）へ
//     接続し、Sensor LocationをRead、RSC MeasurementのNotificationを購読して、
//     speed/cadence/stride/distanceをデコードする。
#include <EspBle.h>

static constexpr const char *RSC_SERVICE_UUID = "1814";
static constexpr const char *RSC_MEASUREMENT_UUID = "2a53";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;

static uint16_t readU16(const String &value, size_t offset)
{
  return static_cast<uint8_t>(value[offset]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
}

static uint32_t readU32(const String &value, size_t offset)
{
  return static_cast<uint32_t>(static_cast<uint8_t>(value[offset])) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 1])) << 8) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 2])) << 16) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 3])) << 24);
}

void setup()
{
  Serial.begin(115200);
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    ble.readCharacteristic(connection.id, RSC_SERVICE_UUID, SENSOR_LOCATION_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 1)
    {
      Serial.printf("Sensor location: %u\n", static_cast<uint8_t>(result.value[0]));
      ble.subscribe(result.connectionId, RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(RSC_MEASUREMENT_UUID) ||
        notification.value.length() < 4)
      return;
    const String &value = notification.value;
    const uint8_t flags = static_cast<uint8_t>(value[0]);
    const double speed = readU16(value, 1) / 256.0; // m/s
    const uint8_t cadence = static_cast<uint8_t>(value[3]);
    Serial.printf("%s: %.2f m/s, cadence %u /min", (flags & 0x04) ? "Running" : "Walking", speed, cadence);
    size_t offset = 4;
    if ((flags & 0x01) && value.length() >= offset + 2)
    {
      Serial.printf(", stride %.2f m", readU16(value, offset) / 100.0);
      offset += 2;
    }
    if ((flags & 0x02) && value.length() >= offset + 4)
    {
      Serial.printf(", distance %.1f m", readU32(value, offset) / 10.0);
    }
    Serial.println();
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(RSC_SERVICE_UUID))
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
