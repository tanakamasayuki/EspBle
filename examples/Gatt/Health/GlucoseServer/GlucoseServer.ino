// en: Record Access Control Point: RACP write → Measurement notify → RACP indicate
// ja: Record Access Control Point: RACP write → Measurement notify → RACP indicate
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *GLUCOSE_SERVICE_UUID = "1808";
static constexpr const char *GLUCOSE_MEASUREMENT_UUID = "2a18";
static constexpr const char *GLUCOSE_FEATURE_UUID = "2a51";
static constexpr const char *RACP_UUID = "2a52";

EspBle ble;
EspBleGattService glucoseServiceService;
EspBleGattCharacteristic glucoseMeasurementCharacteristic;
EspBleGattCharacteristic glucoseFeatureCharacteristic;
EspBleGattCharacteristic racpCharacteristic;
const uint8_t feature[2] = {0x00, 0x00};
enum RacpState { RACP_IDLE, RACP_SEND_MEASUREMENT, RACP_SEND_RESPONSE };
RacpState racpState = RACP_IDLE;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  EspBleGattCharacteristicConfig racpConfig;
  racpConfig.writable = true;
  racpConfig.indicatable = true;
  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  glucoseServiceService = server.addService(GLUCOSE_SERVICE_UUID);
  glucoseMeasurementCharacteristic = server.addCharacteristic(glucoseServiceService, GLUCOSE_MEASUREMENT_UUID, measurementConfig);
  glucoseFeatureCharacteristic = server.addCharacteristic(glucoseServiceService, GLUCOSE_FEATURE_UUID, featureConfig);
  racpCharacteristic = server.addCharacteristic(glucoseServiceService, RACP_UUID, racpConfig);
  server.setValue(glucoseFeatureCharacteristic, feature, sizeof(feature));

  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(RACP_UUID))
      return;
    if (write.value.length() == 2 && static_cast<uint8_t>(write.value[0]) == 0x01)
    {
      uint8_t measurement[13];
      measurement[0] = 0x02; // concentration + type/location present (kg/L)
      measurement[1] = 0x01; // sequence number = 1
      measurement[2] = 0x00;
      measurement[3] = 0xEA; // base time year 2026
      measurement[4] = 0x07;
      measurement[5] = 7;
      measurement[6] = 21;
      measurement[7] = 12;
      measurement[8] = 0;
      measurement[9] = 0;
      espBleWriteMedicalSFloatLE(&measurement[10], 99, 0);
      measurement[12] = 0x11;
      racpState = RACP_SEND_MEASUREMENT;
      // en: Check send acceptance when adapting this example; it is separate from later completion.
      // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
      ble.gattServer().notify(glucoseMeasurementCharacteristic, measurement, sizeof(measurement));
    }
  });
  server.onSent([](const EspBleGattSendResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(GLUCOSE_MEASUREMENT_UUID) &&
        racpState == RACP_SEND_MEASUREMENT)
    {
      racpState = RACP_SEND_RESPONSE;
      const uint8_t response[4] = {0x06, 0x00, 0x01, 0x01};
      ble.gattServer().indicate(racpCharacteristic, response, sizeof(response));
    }
    else if (result.characteristicUuid.equalsIgnoreCase(RACP_UUID))
    {
      racpState = RACP_IDLE;
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Glucose";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(GLUCOSE_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
