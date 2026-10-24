// en: Speed/cadence/stride/distance RSC Measurement notifications, Feature, Sensor Location
// ja: speed/cadence/stride/distance RSC MeasurementのNotify、Feature、Sensor Location
#include <EspBle.h>

static constexpr const char *RSC_SERVICE_UUID = "1814";
static constexpr const char *RSC_MEASUREMENT_UUID = "2a53";
static constexpr const char *RSC_FEATURE_UUID = "2a54";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;
EspBleGattService rscServiceService;
EspBleGattCharacteristic rscMeasurementCharacteristic;
EspBleGattCharacteristic rscFeatureCharacteristic;
EspBleGattCharacteristic sensorLocationCharacteristic;
// en: Stride + Distance
// ja: Stride + Distance
const uint8_t feature[2] = {0x03, 0x00};
const uint8_t sensorLocation = 2;        // en: In Shoe / ja: 靴内
uint32_t totalDistance = 0;
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
  rscServiceService = server.addService(RSC_SERVICE_UUID);
  rscMeasurementCharacteristic = server.addCharacteristic(rscServiceService, RSC_MEASUREMENT_UUID, measurementConfig);
  rscFeatureCharacteristic = server.addCharacteristic(rscServiceService, RSC_FEATURE_UUID, readConfig);
  sensorLocationCharacteristic = server.addCharacteristic(rscServiceService, SENSOR_LOCATION_UUID, readConfig);
  server.setValue(rscFeatureCharacteristic, feature, sizeof(feature));
  server.setValue(sensorLocationCharacteristic, &sensorLocation, 1);

  EspBleConfig config;
  config.deviceName = "EspBle RSC";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(RSC_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every second, notify 3.0 m/s at cadence 180 with a growing distance.
  // ja: 1秒ごとに 3.0 m/s・cadence 180・増加するdistanceをNotifyする。
  if (millis() - lastUpdate >= 1000)
  {
    lastUpdate = millis();
    totalDistance += 30; // en: +3.0 m at 1/10 m units / ja: 1/10 m単位で+3.0 m

    uint8_t measurement[10];
    // en: stride + distance present, walking
    // ja: stride + distance あり、歩行
    measurement[0] = 0x03;
    // en: speed 768 = 3.0 m/s
    // ja: speed 768 = 3.0 m/s
    measurement[1] = 0x00;
    measurement[2] = 0x03;
    measurement[3] = 180;  // cadence
    // en: stride length 1.25 m
    // ja: stride length 1.25 m
    measurement[4] = 125;
    measurement[5] = 0x00;
    measurement[6] = static_cast<uint8_t>(totalDistance & 0xFF);
    measurement[7] = static_cast<uint8_t>((totalDistance >> 8) & 0xFF);
    measurement[8] = static_cast<uint8_t>((totalDistance >> 16) & 0xFF);
    measurement[9] = static_cast<uint8_t>((totalDistance >> 24) & 0xFF);
    ble.gattServer().setValue(rscMeasurementCharacteristic, measurement, sizeof(measurement));
    // en: Check send acceptance when adapting this example; it is separate from later completion.
    // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
    ble.gattServer().notify(rscMeasurementCharacteristic, measurement, sizeof(measurement));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
