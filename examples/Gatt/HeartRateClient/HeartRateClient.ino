#include <EspBle.h>

static constexpr const char *HEART_RATE_SERVICE_UUID = "180d";
static constexpr const char *HEART_RATE_MEASUREMENT_UUID = "2a37";
static constexpr const char *BODY_SENSOR_LOCATION_UUID = "2a38";

EspBle ble;
bool connectionRequested = false;

static bool printMeasurement(const String &value)
{
  if (value.length() < 2) return false;
  const uint8_t flags = static_cast<uint8_t>(value[0]);
  size_t offset = 1;
  uint16_t heartRate = 0;
  if ((flags & 0x01) != 0)
  {
    if (offset + 2 > value.length()) return false;
    heartRate = static_cast<uint8_t>(value[offset]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
    offset += 2;
  }
  else
  {
    heartRate = static_cast<uint8_t>(value[offset++]);
  }

  bool hasEnergy = (flags & 0x08) != 0;
  uint16_t energy = 0;
  if (hasEnergy)
  {
    if (offset + 2 > value.length()) return false;
    energy = static_cast<uint8_t>(value[offset]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
    offset += 2;
  }
  const size_t rrCount = (flags & 0x10) != 0 ? (value.length() - offset) / 2 : 0;
  if ((flags & 0x10) != 0 && ((value.length() - offset) % 2) != 0) return false;
  if ((flags & 0x10) == 0 && offset != value.length()) return false;

  Serial.printf("Heart Rate: %u bpm", heartRate);
  if (hasEnergy) Serial.printf(", energy: %u kJ", energy);
  if (rrCount > 0)
  {
    const uint16_t rr = static_cast<uint8_t>(value[offset]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
    Serial.printf(", RR intervals: %u (first: %u/1024 s)",
      static_cast<unsigned>(rrCount), rr);
  }
  Serial.println();
  return true;
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
    ble.readCharacteristic(
      connection.id, HEART_RATE_SERVICE_UUID, BODY_SENSOR_LOCATION_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.success || result.value.length() != 1)
    {
      Serial.printf("Body Sensor Location read failed: %s\n", result.detail.c_str());
      return;
    }
    Serial.printf("Body Sensor Location: %u\n", static_cast<uint8_t>(result.value[0]));
    ble.subscribe(
      result.connectionId, HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID);
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("Heart Rate subscription: %s\n", result.success ? "ready" : "failed");
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.serviceUuid.equalsIgnoreCase(HEART_RATE_SERVICE_UUID) &&
        notification.characteristicUuid.equalsIgnoreCase(HEART_RATE_MEASUREMENT_UUID) &&
        !printMeasurement(notification.value))
    {
      Serial.println("Invalid Heart Rate Measurement");
    }
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(HEART_RATE_SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
  });

  EspBleScanConfig scan;
  scan.active = true;
  ble.scanner().start(scan);
}

void loop()
{
  ble.update();
  delay(1);
}
