// en: uint16 Weight Measurement indications (0.005 kg resolution) and Feature
// ja: uint16 Weight Measurement（0.005 kg分解能）のIndicateとFeature
#include <EspBle.h>

static constexpr const char *WEIGHT_SCALE_SERVICE_UUID = "181d";
static constexpr const char *WEIGHT_MEASUREMENT_UUID = "2a9d";
static constexpr const char *WEIGHT_SCALE_FEATURE_UUID = "2a9e";

EspBle ble;
EspBleGattService weightScaleServiceService;
EspBleGattCharacteristic weightMeasurementCharacteristic;
EspBleGattCharacteristic weightScaleFeatureCharacteristic;
const uint8_t feature[4] = {0x08, 0x00, 0x00, 0x00};
unsigned long lastUpdate = 0;
uint16_t rawWeight = 14000; // en: 70.000 kg / ja: 70.000 kg

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.indicatable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  weightScaleServiceService = server.addService(WEIGHT_SCALE_SERVICE_UUID);
  weightMeasurementCharacteristic = server.addCharacteristic(weightScaleServiceService, WEIGHT_MEASUREMENT_UUID, measurementConfig);
  weightScaleFeatureCharacteristic = server.addCharacteristic(weightScaleServiceService, WEIGHT_SCALE_FEATURE_UUID, featureConfig);
  server.setValue(weightScaleFeatureCharacteristic, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle Weight Scale";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(WEIGHT_SCALE_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every 3 seconds, indicate a weight that drifts around 70 kg.
  // ja: 3秒ごとに、70 kg付近で変化する体重をIndicateする。
  if (millis() - lastUpdate >= 3000)
  {
    lastUpdate = millis();
    rawWeight += 20; // en: +0.1 kg / ja: +0.1 kg
    if (rawWeight > 14200)
      rawWeight = 14000;

    uint8_t measurement[3];
    // en: SI (kg), no optional fields
    // ja: SI（kg）、optionalなし
    measurement[0] = 0x00;
    measurement[1] = static_cast<uint8_t>(rawWeight & 0xFF);
    measurement[2] = static_cast<uint8_t>((rawWeight >> 8) & 0xFF);
    ble.gattServer().setValue(weightMeasurementCharacteristic, measurement, sizeof(measurement));
    // en: Check send acceptance when adapting this example; it is separate from later completion.
    // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
    ble.gattServer().indicate(weightMeasurementCharacteristic, measurement, sizeof(measurement));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
