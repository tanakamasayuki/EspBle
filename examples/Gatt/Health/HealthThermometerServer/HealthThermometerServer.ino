// en: IEEE-11073 FLOAT Temperature Measurement indications and Temperature Type
// ja: IEEE-11073 FLOAT Temperature MeasurementのIndicateとTemperature Type
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *HEALTH_THERMOMETER_SERVICE_UUID = "1809";
static constexpr const char *TEMPERATURE_MEASUREMENT_UUID = "2a1c";
static constexpr const char *TEMPERATURE_TYPE_UUID = "2a1d";

EspBle ble;
EspBleGattService healthThermometerServiceService;
EspBleGattCharacteristic temperatureMeasurementCharacteristic;
EspBleGattCharacteristic temperatureTypeCharacteristic;
const uint8_t temperatureType = 0x02; // en: Body / ja: 体温
int16_t temperatureHundredths = 3650; // en: 36.50 C / ja: 36.50 度
unsigned long lastUpdate = 0;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.indicatable = true;
  EspBleGattCharacteristicConfig typeConfig;
  typeConfig.readable = true;
  auto &server = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  healthThermometerServiceService = server.addService(HEALTH_THERMOMETER_SERVICE_UUID);
  temperatureMeasurementCharacteristic = server.addCharacteristic(healthThermometerServiceService, TEMPERATURE_MEASUREMENT_UUID, measurementConfig);
  temperatureTypeCharacteristic = server.addCharacteristic(healthThermometerServiceService, TEMPERATURE_TYPE_UUID, typeConfig);
  server.setValue(temperatureTypeCharacteristic, &temperatureType, 1);

  EspBleConfig config;
  config.deviceName = "EspBle Thermometer";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(HEALTH_THERMOMETER_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every 2 seconds, indicate a slowly rising temperature (36.50-38.49 C).
  // ja: 2秒ごとに、緩やかに上昇する温度（36.50〜38.49度）をIndicateする。
  if (millis() - lastUpdate >= 2000)
  {
    lastUpdate = millis();
    temperatureHundredths += 10;
    if (temperatureHundredths > 3849)
      temperatureHundredths = 3650;

    uint8_t measurement[5];
    // en: Celsius, no timestamp/type
    // ja: 摂氏、timestamp/type無し
    measurement[0] = 0x00;
    espBleWriteMedicalFloat32LE(&measurement[1], temperatureHundredths, -2);
    ble.gattServer().setValue(temperatureMeasurementCharacteristic, measurement, sizeof(measurement));
    // en: indicate() only reaches subscribers; it is a no-op with none.
    // ja: indicate()は購読者にのみ届く。購読者がいなければ何もしない。
    ble.gattServer().indicate(temperatureMeasurementCharacteristic, measurement, sizeof(measurement));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
