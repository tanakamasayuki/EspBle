// en: LocationNavigationClient - connect to a Location and Navigation Service
//     (0x1819), read LN Feature, subscribe to Location and Speed notifications,
//     and decode Instantaneous Speed and the Location latitude/longitude.
// ja: LocationNavigationClient - Location and Navigation Service（0x1819）へ接続
//     し、LN FeatureをRead、Location and SpeedのNotificationを購読して、
//     Instantaneous SpeedとLocationの緯度・経度をデコードする。
#include <EspBle.h>

static constexpr const char *LN_SERVICE_UUID = "1819";
static constexpr const char *LOCATION_AND_SPEED_UUID = "2a67";
static constexpr const char *LN_FEATURE_UUID = "2a6a";

EspBle ble;

static uint16_t readU16(const String &value, size_t offset)
{
  return static_cast<uint8_t>(value[offset]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
}

static int32_t readS32(const String &value, size_t offset)
{
  const uint32_t raw = static_cast<uint32_t>(static_cast<uint8_t>(value[offset])) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 1])) << 8) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 2])) << 16) |
    (static_cast<uint32_t>(static_cast<uint8_t>(value[offset + 3])) << 24);
  return static_cast<int32_t>(raw);
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
    ble.readCharacteristic(connection.id, LN_SERVICE_UUID, LN_FEATURE_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 4)
    {
      ble.subscribe(result.connectionId, LN_SERVICE_UUID, LOCATION_AND_SPEED_UUID);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(LOCATION_AND_SPEED_UUID) ||
        notification.value.length() < 12)
      return;
    const String &value = notification.value;
    const uint16_t flags = readU16(value, 0);
    // en: This example emits Instantaneous Speed (bit 0) then Location (bit 2).
    // ja: この例はInstantaneous Speed（bit 0）に続いてLocation（bit 2）を出す。
    if ((flags & 0x0005) != 0x0005)
      return;
    const double speed = readU16(value, 2) * 0.01; // en: 1/100 m/s / ja: 1/100 m/s
    const double latitude = readS32(value, 4) * 1e-7;  // en: 1e-7 deg / ja: 1e-7 度
    const double longitude = readS32(value, 8) * 1e-7; // en: 1e-7 deg / ja: 1e-7 度
    Serial.printf("Speed: %.2f m/s, location: %.6f, %.6f\n", speed, latitude, longitude);
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(LN_SERVICE_UUID))
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
