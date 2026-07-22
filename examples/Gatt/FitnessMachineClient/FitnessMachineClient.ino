// en: FitnessMachineClient - connect to a Fitness Machine Service (0x1826), read
//     Fitness Machine Feature, subscribe to Indoor Bike Data (0x2AD2), and
//     decode the flags-driven instantaneous speed / cadence / power.
// ja: FitnessMachineClient - Fitness Machine Service（0x1826）へ接続し、Fitness
//     Machine FeatureをRead、Indoor Bike Data（0x2AD2）を購読して、flagsに従う
//     instantaneous speed / cadence / power をデコードする。
#include <EspBle.h>

static constexpr const char *FTMS_SERVICE_UUID = "1826";
static constexpr const char *INDOOR_BIKE_DATA_UUID = "2ad2";
static constexpr const char *FITNESS_MACHINE_FEATURE_UUID = "2acc";

EspBle ble;

static uint16_t u16(const String &value, size_t offset)
{
  return static_cast<uint8_t>(value[offset]) |
    (static_cast<uint16_t>(static_cast<uint8_t>(value[offset + 1])) << 8);
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
    ble.readCharacteristic(connection.id, FTMS_SERVICE_UUID, FITNESS_MACHINE_FEATURE_UUID);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success && result.value.length() == 8)
    {
      Serial.println("Fitness Machine Feature read; subscribing to Indoor Bike Data");
      ble.subscribe(result.connectionId, FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID);
    }
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(INDOOR_BIKE_DATA_UUID) ||
        notification.value.length() < 2)
      return;
    const String &value = notification.value;
    const uint16_t flags = u16(value, 0);
    size_t offset = 2;
    uint16_t speed = 0, cadence = 0;
    int16_t power = 0;
    if (!(flags & 0x0001)) { speed = u16(value, offset); offset += 2; } // Instantaneous Speed
    if (flags & 0x0002) offset += 2;                                    // Average Speed
    if (flags & 0x0004) { cadence = u16(value, offset); offset += 2; }  // Instantaneous Cadence
    if (flags & 0x0008) offset += 2;                                    // Average Cadence
    if (flags & 0x0010) offset += 3;                                    // Total Distance
    if (flags & 0x0020) offset += 2;                                    // Resistance Level
    if (flags & 0x0040) { power = static_cast<int16_t>(u16(value, offset)); offset += 2; } // Instantaneous Power
    // Speed is 0.01 km/h, cadence is 0.5 /min (so rpm = cadence / 2).
    Serial.printf("Speed: %u.%02u km/h  Cadence: %u rpm  Power: %d W\n",
      speed / 100, speed % 100, cadence / 2, power);
  });

  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(FTMS_SERVICE_UUID))
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
