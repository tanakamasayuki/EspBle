// en: SFLOAT SpO2/pulse-rate Spot-Check Measurement indications and Features
// ja: SFLOAT SpO2/pulse-rate Spot-Check MeasurementのIndicateとFeatures
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *PLX_SERVICE_UUID = "1822";
static constexpr const char *PLX_SPOT_CHECK_UUID = "2a5e";
static constexpr const char *PLX_FEATURES_UUID = "2a60";

EspBle ble;
EspBleGattService plxServiceService;
EspBleGattCharacteristic plxSpotCheckCharacteristic;
EspBleGattCharacteristic plxFeaturesCharacteristic;
const uint8_t features[2] = {0x03, 0x00};
int spo2 = 98;
unsigned long lastUpdate = 0;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig spotCheckConfig;
  spotCheckConfig.indicatable = true;
  EspBleGattCharacteristicConfig featuresConfig;
  featuresConfig.readable = true;
  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  plxServiceService = server.addService(PLX_SERVICE_UUID);
  plxSpotCheckCharacteristic = server.addCharacteristic(plxServiceService, PLX_SPOT_CHECK_UUID, spotCheckConfig);
  plxFeaturesCharacteristic = server.addCharacteristic(plxServiceService, PLX_FEATURES_UUID, featuresConfig);
  server.setValue(plxFeaturesCharacteristic, features, sizeof(features));

  EspBleConfig config;
  config.deviceName = "EspBle Pulse Oximeter";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(PLX_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every 3 seconds, indicate SpO2 (95-99 %) and a 60 bpm pulse rate.
  // ja: 3秒ごとに SpO2（95〜99 %）と 60 bpm の pulse rate をIndicateする。
  if (millis() - lastUpdate >= 3000)
  {
    lastUpdate = millis();
    spo2 += 1;
    if (spo2 > 99)
      spo2 = 95;

    uint8_t measurement[5];
    measurement[0] = 0x00; // en: no optional fields / ja: optionalなし
    espBleWriteMedicalSFloatLE(&measurement[1], static_cast<int16_t>(spo2), 0);
    espBleWriteMedicalSFloatLE(&measurement[3], 60, 0);
    ble.gattServer().setValue(plxSpotCheckCharacteristic, measurement, sizeof(measurement));
    // en: Check send acceptance when adapting this example; it is separate from later completion.
    // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
    ble.gattServer().indicate(plxSpotCheckCharacteristic, measurement, sizeof(measurement));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
