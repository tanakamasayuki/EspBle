// en: CyclingPowerServer - standard Cycling Power Service (0x1818). Cycling
//     Power Measurement (0x2A63) is notified with 16-bit flags and a signed
//     16-bit instantaneous power (watts); Cycling Power Feature (0x2A65) and
//     Sensor Location (0x2A5D) are readable.
// ja: CyclingPowerServer - 標準Cycling Power Service（0x1818）。Cycling Power
//     Measurement（0x2A63）を16bit flagsと符号付き16bit instantaneous power
//     （ワット）でNotifyし、Cycling Power Feature（0x2A65）とSensor Location
//     （0x2A5D）はReadできる。
#include <EspBle.h>

static constexpr const char *CYCLING_POWER_SERVICE_UUID = "1818";
static constexpr const char *CYCLING_POWER_MEASUREMENT_UUID = "2a63";
static constexpr const char *CYCLING_POWER_FEATURE_UUID = "2a65";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;
const uint8_t feature[4] = {0x0C, 0x00, 0x00, 0x00};
const uint8_t sensorLocation = 6; // en: Right Crank / ja: 右クランク
int16_t power = 200;
unsigned long lastUpdate = 0;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig readConfig;
  readConfig.readable = true;
  auto &server = ble.gattServer();
  server.addService(CYCLING_POWER_SERVICE_UUID);
  server.addCharacteristic(CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID, measurementConfig);
  server.addCharacteristic(CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_FEATURE_UUID, readConfig);
  server.addCharacteristic(CYCLING_POWER_SERVICE_UUID, SENSOR_LOCATION_UUID, readConfig);
  server.setValue(CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_FEATURE_UUID, feature, sizeof(feature));
  server.setValue(CYCLING_POWER_SERVICE_UUID, SENSOR_LOCATION_UUID, &sensorLocation, 1);

  EspBleConfig config;
  config.deviceName = "EspBle Cycling Power";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(CYCLING_POWER_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every second, notify a power that ramps 200..300 W.
  // ja: 1秒ごとに 200〜300 W を上下するpowerをNotifyする。
  if (millis() - lastUpdate >= 1000)
  {
    lastUpdate = millis();
    power += 10;
    if (power > 300)
      power = 200;

    uint8_t measurement[4];
    measurement[0] = 0x00; // en: 16-bit flags, no optional fields / ja: 16bit flags、optionalなし
    measurement[1] = 0x00;
    measurement[2] = static_cast<uint8_t>(static_cast<uint16_t>(power) & 0xFF);
    measurement[3] = static_cast<uint8_t>((static_cast<uint16_t>(power) >> 8) & 0xFF);
    ble.gattServer().setValue(CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID, measurement, sizeof(measurement));
    ble.gattServer().notify(CYCLING_POWER_SERVICE_UUID, CYCLING_POWER_MEASUREMENT_UUID, measurement, sizeof(measurement));
  }

  ble.update();
  delay(1);
}
