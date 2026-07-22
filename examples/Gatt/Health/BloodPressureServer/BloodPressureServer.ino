// en: BloodPressureServer - standard Blood Pressure Service (0x1810). Blood
//     Pressure Measurement (0x2A35) is indicated with systolic/diastolic/mean
//     as IEEE-11073 16-bit SFLOATs; Blood Pressure Feature (0x2A49) is readable.
// ja: BloodPressureServer - 標準Blood Pressure Service（0x1810）。Blood Pressure
//     Measurement（0x2A35）をsystolic/diastolic/meanのIEEE-11073 16-bit SFLOATで
//     Indicateし、Blood Pressure Feature（0x2A49）はReadできる。
#include <EspBle.h>
#include <EspBleMedicalFloat.h>

static constexpr const char *BLOOD_PRESSURE_SERVICE_UUID = "1810";
static constexpr const char *BLOOD_PRESSURE_MEASUREMENT_UUID = "2a35";
static constexpr const char *BLOOD_PRESSURE_FEATURE_UUID = "2a49";

EspBle ble;
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
  server.addService(BLOOD_PRESSURE_SERVICE_UUID);
  server.addCharacteristic(BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID, measurementConfig);
  server.addCharacteristic(BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_FEATURE_UUID, featureConfig);
  server.setValue(BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_FEATURE_UUID, feature, sizeof(feature));

  EspBleConfig config;
  config.deviceName = "EspBle Blood Pressure";
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
    ble.gattServer().setValue(
      BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID, measurement, sizeof(measurement));
    ble.gattServer().indicate(
      BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID, measurement, sizeof(measurement));
  }

  ble.update();
  delay(1);
}
