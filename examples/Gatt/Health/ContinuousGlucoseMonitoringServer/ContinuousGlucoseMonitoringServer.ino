// en: E2E-CRC-protected CGM Feature and CGM Measurement notifications
// ja: E2E-CRC保護のCGM FeatureとCGM MeasurementのNotify
#include <EspBle.h>
#include <EspBleCgmCrc.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *CGM_SERVICE_UUID = "181f";
static constexpr const char *CGM_MEASUREMENT_UUID = "2aa7";
static constexpr const char *CGM_FEATURE_UUID = "2aa8";

EspBle ble;
EspBleGattService cgmServiceService;
EspBleGattCharacteristic cgmMeasurementCharacteristic;
EspBleGattCharacteristic cgmFeatureCharacteristic;
unsigned long lastUpdate = 0;
// en: minutes since session start
// ja: セッション開始からの分
uint16_t timeOffset = 5;
int glucose = 100;       // en: mg/dL / ja: mg/dL

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  uint8_t feature[6];
  // en: bit 12 = E2E-CRC supported (0x001000)
  // ja: bit 12 = E2E-CRC対応
  feature[0] = 0x00;
  feature[1] = 0x10;
  feature[2] = 0x00;
  // en: Type = Capillary Whole blood, Location = Finger
  // ja: Type=毛細血、Location=指
  feature[3] = 0x11;
  espBleCgmAppendCrc(feature, 4);
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  cgmServiceService = server.addService(CGM_SERVICE_UUID);
  cgmMeasurementCharacteristic = server.addCharacteristic(cgmServiceService, CGM_MEASUREMENT_UUID, measurementConfig);
  cgmFeatureCharacteristic = server.addCharacteristic(cgmServiceService, CGM_FEATURE_UUID, featureConfig);
  server.setValue(cgmFeatureCharacteristic, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle CGM";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(CGM_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every 3 seconds, notify a CGM Measurement with a fresh time offset and a
  //     glucose value drifting around 100 mg/dL, each protected by an E2E-CRC.
  // ja: 3秒ごとに、新しいtime offsetと100 mg/dL付近で変化する血糖値を持つCGM
  //     MeasurementをE2E-CRC付きでNotifyする。
  if (millis() - lastUpdate >= 3000)
  {
    lastUpdate = millis();
    timeOffset += 5;
    glucose += 1;
    if (glucose > 110)
      glucose = 100;

    uint8_t measurement[8];
    measurement[0] = 8;    // en: Size / ja: Size
    measurement[1] = 0x00; // en: Flags / ja: Flags
    espBleWriteMedicalSFloatLE(&measurement[2], glucose, 0);
    measurement[4] = static_cast<uint8_t>(timeOffset & 0xFF);
    measurement[5] = static_cast<uint8_t>((timeOffset >> 8) & 0xFF);
    espBleCgmAppendCrc(measurement, 6);
    ble.gattServer().setValue(cgmMeasurementCharacteristic, measurement, sizeof(measurement));
    // en: Check send acceptance when adapting this example; it is separate from later completion.
    // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
    ble.gattServer().notify(cgmMeasurementCharacteristic, measurement, sizeof(measurement));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
