// en: LocationNavigationServer - standard Location and Navigation Service
//     (0x1819). Location and Speed (0x2A67) is notified with uint16 flags plus
//     flags-selected fields; LN Feature (0x2A6A) is a readable uint32.
// ja: LocationNavigationServer - 標準Location and Navigation Service（0x1819）。
//     Location and Speed（0x2A67）をuint16 flags＋flagsで選ばれるフィールド付きで
//     Notifyし、LN Feature（0x2A6A）はuint32としてReadできる。
#include <EspBle.h>

static constexpr const char *LN_SERVICE_UUID = "1819";
static constexpr const char *LOCATION_AND_SPEED_UUID = "2a67";
static constexpr const char *LN_FEATURE_UUID = "2a6a";

EspBle ble;
// en: bit 0 = Instantaneous Speed, bit 2 = Location / ja: bit 0 = 速度、bit 2 = 位置
const uint8_t feature[4] = {0x05, 0x00, 0x00, 0x00};
unsigned long lastUpdate = 0;
uint16_t speed = 500; // en: 5.00 m/s / ja: 5.00 m/s

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  server.addService(LN_SERVICE_UUID);
  server.addCharacteristic(LN_SERVICE_UUID, LOCATION_AND_SPEED_UUID, measurementConfig);
  server.addCharacteristic(LN_SERVICE_UUID, LN_FEATURE_UUID, featureConfig);
  server.setValue(LN_SERVICE_UUID, LN_FEATURE_UUID, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle Location Nav";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(LN_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every second, notify a speed drifting near 5 m/s plus a fixed Tokyo
  //     location (flags 0x0005: Instantaneous Speed + Location).
  // ja: 1秒ごとに、5 m/s付近で変化する速度と固定の東京の位置（flags 0x0005:
  //     速度＋位置）をNotifyする。
  if (millis() - lastUpdate >= 1000)
  {
    lastUpdate = millis();
    speed += 10; // en: +0.1 m/s / ja: +0.1 m/s
    if (speed > 600)
      speed = 500;

    const uint16_t flags = 0x0005;
    const int32_t latitude = 356812000;   // en: 35.6812 deg / ja: 35.6812 度
    const int32_t longitude = 1397671000; // en: 139.7671 deg / ja: 139.7671 度
    const uint32_t lat = static_cast<uint32_t>(latitude);
    const uint32_t lon = static_cast<uint32_t>(longitude);
    uint8_t measurement[12];
    measurement[0] = static_cast<uint8_t>(flags & 0xFF);
    measurement[1] = static_cast<uint8_t>((flags >> 8) & 0xFF);
    measurement[2] = static_cast<uint8_t>(speed & 0xFF);
    measurement[3] = static_cast<uint8_t>((speed >> 8) & 0xFF);
    measurement[4] = static_cast<uint8_t>(lat & 0xFF);
    measurement[5] = static_cast<uint8_t>((lat >> 8) & 0xFF);
    measurement[6] = static_cast<uint8_t>((lat >> 16) & 0xFF);
    measurement[7] = static_cast<uint8_t>((lat >> 24) & 0xFF);
    measurement[8] = static_cast<uint8_t>(lon & 0xFF);
    measurement[9] = static_cast<uint8_t>((lon >> 8) & 0xFF);
    measurement[10] = static_cast<uint8_t>((lon >> 16) & 0xFF);
    measurement[11] = static_cast<uint8_t>((lon >> 24) & 0xFF);
    ble.gattServer().setValue(LN_SERVICE_UUID, LOCATION_AND_SPEED_UUID, measurement, sizeof(measurement));
    ble.gattServer().notify(LN_SERVICE_UUID, LOCATION_AND_SPEED_UUID, measurement, sizeof(measurement));
  }

  ble.update();
  delay(1);
}
