// en: Multi-field wheel/crank CSC Measurement notifications, Feature, Sensor Location
// ja: 多フィールドwheel/crank CSC MeasurementのNotify、Feature、Sensor Location
#include <EspBle.h>

static constexpr const char *CSC_SERVICE_UUID = "1816";
static constexpr const char *CSC_MEASUREMENT_UUID = "2a5b";
static constexpr const char *CSC_FEATURE_UUID = "2a5c";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;
EspBleGattService cscServiceService;
EspBleGattCharacteristic cscMeasurementCharacteristic;
EspBleGattCharacteristic cscFeatureCharacteristic;
EspBleGattCharacteristic sensorLocationCharacteristic;
const uint8_t feature[2] = {0x03, 0x00}; // en: Wheel + Crank / ja: Wheel + Crank
const uint8_t sensorLocation = 12;       // en: Rear Hub / ja: リアハブ
uint32_t wheelRevolutions = 0;
uint16_t crankRevolutions = 0;
uint16_t eventTime = 0;
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
  cscServiceService = server.addService(CSC_SERVICE_UUID);
  cscMeasurementCharacteristic = server.addCharacteristic(cscServiceService, CSC_MEASUREMENT_UUID, measurementConfig);
  cscFeatureCharacteristic = server.addCharacteristic(cscServiceService, CSC_FEATURE_UUID, readConfig);
  sensorLocationCharacteristic = server.addCharacteristic(cscServiceService, SENSOR_LOCATION_UUID, readConfig);
  server.setValue(cscFeatureCharacteristic, feature, sizeof(feature));
  server.setValue(sensorLocationCharacteristic, &sensorLocation, 1);

  EspBleConfig config;
  config.deviceName = "EspBle CSC";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(CSC_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every second, advance the counters and notify a measurement.
  // ja: 1秒ごとにカウンタを進めてMeasurementをNotifyする。
  if (millis() - lastUpdate >= 1000)
  {
    lastUpdate = millis();
    wheelRevolutions += 2;
    crankRevolutions += 1;
    eventTime += 1024; // en: +1 s at 1/1024 units / ja: 1/1024単位で+1秒

    uint8_t measurement[11];
    // en: wheel + crank present
    // ja: wheel + crank あり
    measurement[0] = 0x03;
    measurement[1] = static_cast<uint8_t>(wheelRevolutions & 0xFF);
    measurement[2] = static_cast<uint8_t>((wheelRevolutions >> 8) & 0xFF);
    measurement[3] = static_cast<uint8_t>((wheelRevolutions >> 16) & 0xFF);
    measurement[4] = static_cast<uint8_t>((wheelRevolutions >> 24) & 0xFF);
    measurement[5] = static_cast<uint8_t>(eventTime & 0xFF);
    measurement[6] = static_cast<uint8_t>((eventTime >> 8) & 0xFF);
    measurement[7] = static_cast<uint8_t>(crankRevolutions & 0xFF);
    measurement[8] = static_cast<uint8_t>((crankRevolutions >> 8) & 0xFF);
    measurement[9] = static_cast<uint8_t>(eventTime & 0xFF);
    measurement[10] = static_cast<uint8_t>((eventTime >> 8) & 0xFF);
    ble.gattServer().setValue(cscMeasurementCharacteristic, measurement, sizeof(measurement));
    // en: Check send acceptance when adapting this example; it is separate from later completion.
    // ja: 組み込む際は送信要求の受理を確認します。後から届く完了とは別です。
    ble.gattServer().notify(cscMeasurementCharacteristic, measurement, sizeof(measurement));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
