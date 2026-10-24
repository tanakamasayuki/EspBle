// en: Body Fat Percentage + optional Weight Measurement indications and Feature
// ja: Body Fat Percentage＋任意Weight MeasurementのIndicateとFeature
#include <EspBle.h>

static constexpr const char *BODY_COMPOSITION_SERVICE_UUID = "181b";
static constexpr const char *BODY_COMPOSITION_MEASUREMENT_UUID = "2a9c";
static constexpr const char *BODY_COMPOSITION_FEATURE_UUID = "2a9b";

EspBle ble;
EspBleGattService bodyCompositionServiceService;
EspBleGattCharacteristic bodyCompositionMeasurementCharacteristic;
EspBleGattCharacteristic bodyCompositionFeatureCharacteristic;
// en: bit 9 = Weight supported
// ja: bit 9 = Weight対応
const uint8_t feature[4] = {0x00, 0x02, 0x00, 0x00};
unsigned long lastUpdate = 0;
uint16_t rawFat = 275; // en: 27.5 % / ja: 27.5 %

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
  bodyCompositionServiceService = server.addService(BODY_COMPOSITION_SERVICE_UUID);
  bodyCompositionMeasurementCharacteristic = server.addCharacteristic(bodyCompositionServiceService, BODY_COMPOSITION_MEASUREMENT_UUID, measurementConfig);
  bodyCompositionFeatureCharacteristic = server.addCharacteristic(bodyCompositionServiceService, BODY_COMPOSITION_FEATURE_UUID, featureConfig);
  server.setValue(bodyCompositionFeatureCharacteristic, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle Body Composition";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(BODY_COMPOSITION_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every 3 seconds, indicate body fat drifting near 27.5 % plus a fixed
  //     70.000 kg weight (flag bit 10 set).
  // ja: 3秒ごとに、27.5 %付近で変化する体脂肪率と固定の70.000 kg体重（flag bit
  //     10をセット）をIndicateする。
  if (millis() - lastUpdate >= 3000)
  {
    lastUpdate = millis();
    rawFat += 1; // en: +0.1 % / ja: +0.1 %
    if (rawFat > 285)
      rawFat = 275;

    const uint16_t flags = 0x0400; // en: SI units, Weight present / ja: SI単位、Weightあり
    const uint16_t rawWeight = 14000; // en: 70.000 kg / ja: 70.000 kg
    uint8_t measurement[6];
    measurement[0] = static_cast<uint8_t>(flags & 0xFF);
    measurement[1] = static_cast<uint8_t>((flags >> 8) & 0xFF);
    measurement[2] = static_cast<uint8_t>(rawFat & 0xFF);
    measurement[3] = static_cast<uint8_t>((rawFat >> 8) & 0xFF);
    measurement[4] = static_cast<uint8_t>(rawWeight & 0xFF);
    measurement[5] = static_cast<uint8_t>((rawWeight >> 8) & 0xFF);
    ble.gattServer().setValue(bodyCompositionMeasurementCharacteristic, measurement, sizeof(measurement));
    // en: Check send acceptance when adapting this example; it is separate from later completion.
    // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
    ble.gattServer().indicate(bodyCompositionMeasurementCharacteristic, measurement, sizeof(measurement));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
