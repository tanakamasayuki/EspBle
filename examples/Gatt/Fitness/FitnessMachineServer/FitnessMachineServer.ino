// en: Fitness Machine (FTMS) Indoor Bike Data notifications and Feature
// ja: Fitness Machine（FTMS）Indoor Bike DataのNotifyとFeature
#include <EspBle.h>

static constexpr const char *FTMS_SERVICE_UUID = "1826";
static constexpr const char *INDOOR_BIKE_DATA_UUID = "2ad2";
static constexpr const char *FITNESS_MACHINE_FEATURE_UUID = "2acc";

EspBle ble;
EspBleGattService ftmsServiceService;
EspBleGattCharacteristic indoorBikeDataCharacteristic;
EspBleGattCharacteristic fitnessMachineFeatureCharacteristic;
// en: Fitness Machine Features = 0x00000006 (Cadence + Total Distance supported).
// ja: Fitness Machine Featuresの0x00000006は、CadenceとTotal Distanceへの対応を示します。
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
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  ftmsServiceService = server.addService(FTMS_SERVICE_UUID);
  indoorBikeDataCharacteristic = server.addCharacteristic(ftmsServiceService, INDOOR_BIKE_DATA_UUID, measurementConfig);
  fitnessMachineFeatureCharacteristic = server.addCharacteristic(ftmsServiceService, FITNESS_MACHINE_FEATURE_UUID, readConfig);
  server.setValue(fitnessMachineFeatureCharacteristic, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle Fitness Machine";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
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
    ble.gattServer().setValue(indoorBikeDataCharacteristic, data, sizeof(data));
    // en: Check send acceptance when adapting this example; it is separate from later completion.
    // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
    ble.gattServer().notify(indoorBikeDataCharacteristic, data, sizeof(data));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
