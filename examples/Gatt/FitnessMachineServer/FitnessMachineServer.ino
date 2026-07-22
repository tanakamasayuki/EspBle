// en: FitnessMachineServer - standard Fitness Machine Service (0x1826) as used
//     by smart trainers / indoor bikes. Indoor Bike Data (0x2AD2) is notified
//     with 16-bit flags followed by instantaneous speed (0.01 km/h), cadence
//     (0.5 /min), and signed power (W); Fitness Machine Feature (0x2ACC) is a
//     readable 8-byte pair of feature bitmaps.
// ja: FitnessMachineServer - スマートトレーナー/屋内バイクで使われる標準Fitness
//     Machine Service（0x1826）。Indoor Bike Data（0x2AD2）を16bit flags＋
//     instantaneous speed（0.01 km/h）・cadence（0.5/min）・符号付きpower（W）で
//     Notifyし、Fitness Machine Feature（0x2ACC）は8byteのfeature bitmap対をReadできる。
#include <EspBle.h>

static constexpr const char *FTMS_SERVICE_UUID = "1826";
static constexpr const char *INDOOR_BIKE_DATA_UUID = "2ad2";
static constexpr const char *FITNESS_MACHINE_FEATURE_UUID = "2acc";

EspBle ble;
// Fitness Machine Features = 0x00000006 (Cadence + Total Distance supported).
const uint8_t feature[8] = {0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint16_t speed = 3000; // en: 0.01 km/h units / ja: 0.01 km/h単位
unsigned long lastUpdate = 0;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig readConfig;
  readConfig.readable = true;
  auto &server = ble.gattServer();
  server.addService(FTMS_SERVICE_UUID);
  server.addCharacteristic(FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID, measurementConfig);
  server.addCharacteristic(FTMS_SERVICE_UUID, FITNESS_MACHINE_FEATURE_UUID, readConfig);
  server.setValue(FTMS_SERVICE_UUID, FITNESS_MACHINE_FEATURE_UUID, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle Fitness Machine";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(FTMS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every second, notify Indoor Bike Data with speed ramping 30..40 km/h,
  //     a fixed 90 rpm cadence, and 250 W power.
  // ja: 1秒ごとに 30〜40 km/h を上下するspeed、cadence 90 rpm、power 250 W を
  //     Indoor Bike DataとしてNotifyする。
  if (millis() - lastUpdate >= 1000)
  {
    lastUpdate = millis();
    speed += 100;
    if (speed > 4000)
      speed = 3000;

    const uint16_t flags = 0x0044;   // Instantaneous Speed (bit0=0) + Cadence (bit2) + Power (bit6)
    const uint16_t cadence = 180;    // 90 rpm in 0.5/min units
    const int16_t power = 250;       // watts
    uint8_t data[8];
    data[0] = static_cast<uint8_t>(flags & 0xFF);
    data[1] = static_cast<uint8_t>((flags >> 8) & 0xFF);
    data[2] = static_cast<uint8_t>(speed & 0xFF);
    data[3] = static_cast<uint8_t>((speed >> 8) & 0xFF);
    data[4] = static_cast<uint8_t>(cadence & 0xFF);
    data[5] = static_cast<uint8_t>((cadence >> 8) & 0xFF);
    data[6] = static_cast<uint8_t>(static_cast<uint16_t>(power) & 0xFF);
    data[7] = static_cast<uint8_t>((static_cast<uint16_t>(power) >> 8) & 0xFF);
    ble.gattServer().setValue(FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID, data, sizeof(data));
    ble.gattServer().notify(FTMS_SERVICE_UUID, INDOOR_BIKE_DATA_UUID, data, sizeof(data));
  }

  ble.update();
  delay(1);
}
