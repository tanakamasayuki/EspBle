// en: CyclingSpeedCadenceClient - connect to a Cycling Speed and Cadence
//     Service (0x1816), read Sensor Location, subscribe to CSC Measurement
//     notifications, and decode the wheel/crank revolution fields.
// ja: CyclingSpeedCadenceClient - Cycling Speed and Cadence Service（0x1816）へ
//     接続し、Sensor LocationをRead、CSC MeasurementのNotificationを購読して、
//     wheel/crank回転数フィールドをデコードする。
#include <EspBle.h>

static constexpr const char *CSC_SERVICE_UUID = "1816";
static constexpr const char *CSC_MEASUREMENT_UUID = "2a5b";
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
    ble.readCharacteristic(connection.id, CSC_SERVICE_UUID, SENSOR_LOCATION_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 1)
    {
      Serial.printf("Sensor location: %u\n", static_cast<uint8_t>(result.value[0]));
      ble.subscribe(result.connectionId, CSC_SERVICE_UUID, CSC_MEASUREMENT_UUID);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(CSC_MEASUREMENT_UUID) ||
        notification.value.length() < 1)
      return;
    const String &value = notification.value;
    const uint8_t flags = static_cast<uint8_t>(value[0]);
    size_t offset = 1;
    if ((flags & 0x01) && value.length() >= offset + 6)
    {
      const uint32_t wheel = readU32(value, offset);
      const uint16_t wheelTime = readU16(value, offset + 4);
      Serial.printf("Wheel: %lu revs, last event %.3f s\n",
        static_cast<unsigned long>(wheel), wheelTime / 1024.0);
      offset += 6;
    }
    if ((flags & 0x02) && value.length() >= offset + 4)
    {
      const uint16_t crank = readU16(value, offset);
      const uint16_t crankTime = readU16(value, offset + 2);
      Serial.printf("Crank: %u revs, last event %.3f s\n", crank, crankTime / 1024.0);
    }
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(CSC_SERVICE_UUID))
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
