// en: IEEE-11073 SFLOAT systolic/diastolic/mean Measurement indications and Feature
// ja: IEEE-11073 SFLOAT systolic/diastolic/meanのMeasurement IndicateとFeature
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *BLOOD_PRESSURE_SERVICE_UUID = "1810";
static constexpr const char *BLOOD_PRESSURE_MEASUREMENT_UUID = "2a35";
static constexpr const char *BLOOD_PRESSURE_FEATURE_UUID = "2a49";

EspBle ble;
EspBleGattService bloodPressureServiceService;
EspBleGattCharacteristic bloodPressureMeasurementCharacteristic;
EspBleGattCharacteristic bloodPressureFeatureCharacteristic;
const uint8_t feature[2] = {0x03, 0x00}; // en: Body Movement + Cuff Fit / ja: 体動＋カフ装着検出
unsigned long lastUpdate = 0;

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
  bloodPressureServiceService = server.addService(BLOOD_PRESSURE_SERVICE_UUID);
  bloodPressureMeasurementCharacteristic = server.addCharacteristic(bloodPressureServiceService, BLOOD_PRESSURE_MEASUREMENT_UUID, measurementConfig);
  bloodPressureFeatureCharacteristic = server.addCharacteristic(bloodPressureServiceService, BLOOD_PRESSURE_FEATURE_UUID, featureConfig);
  server.setValue(bloodPressureFeatureCharacteristic, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle Blood Pressure";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(BLOOD_PRESSURE_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every 3 seconds, indicate a 120/80 mmHg reading (mean 93).
  // ja: 3秒ごとに 120/80 mmHg（平均93）をIndicateする。
  if (millis() - lastUpdate >= 3000)
  {
    lastUpdate = millis();
    uint8_t measurement[7];
    measurement[0] = 0x00; // en: mmHg, no optional fields / ja: mmHg、optionalなし
    espBleWriteMedicalSFloatLE(&measurement[1], 120, 0);
    espBleWriteMedicalSFloatLE(&measurement[3], 80, 0);
    espBleWriteMedicalSFloatLE(&measurement[5], 93, 0);
    ble.gattServer().setValue(bloodPressureMeasurementCharacteristic, measurement, sizeof(measurement));
    // en: Check send acceptance when adapting this example; it is separate from later completion.
    // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
    ble.gattServer().indicate(bloodPressureMeasurementCharacteristic, measurement, sizeof(measurement));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
