// en: RunningSpeedCadenceServer - standard Running Speed and Cadence Service
//     (0x1814). RSC Measurement (0x2A53) is notified with instantaneous speed
//     and cadence plus optional stride length and total distance; RSC Feature
//     (0x2A54) and Sensor Location (0x2A5D) are readable.
// ja: RunningSpeedCadenceServer - 標準Running Speed and Cadence Service
//     （0x1814）。RSC Measurement（0x2A53）を瞬間speed・cadenceと任意のstride
//     length・total distanceでNotifyし、RSC Feature（0x2A54）とSensor Location
//     （0x2A5D）はReadできる。
#include <EspBle.h>

static constexpr const char *RSC_SERVICE_UUID = "1814";
static constexpr const char *RSC_MEASUREMENT_UUID = "2a53";
static constexpr const char *RSC_FEATURE_UUID = "2a54";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;
const uint8_t feature[2] = {0x03, 0x00}; // en: Stride + Distance / ja: Stride + Distance
const uint8_t sensorLocation = 2;        // en: In Shoe / ja: 靴内
uint32_t totalDistance = 0;
unsigned long lastUpdate = 0;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig readConfig;
  readConfig.readable = true;
  auto &server = ble.gattServer();
  server.addService(RSC_SERVICE_UUID);
  server.addCharacteristic(RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID, measurementConfig);
  server.addCharacteristic(RSC_SERVICE_UUID, RSC_FEATURE_UUID, readConfig);
  server.addCharacteristic(RSC_SERVICE_UUID, SENSOR_LOCATION_UUID, readConfig);
  server.setValue(RSC_SERVICE_UUID, RSC_FEATURE_UUID, feature, sizeof(feature));
  server.setValue(RSC_SERVICE_UUID, SENSOR_LOCATION_UUID, &sensorLocation, 1);

  EspBleConfig config;
  config.deviceName = "EspBle RSC";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(RSC_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  // en: Every second, notify 3.0 m/s at cadence 180 with a growing distance.
  // ja: 1秒ごとに 3.0 m/s・cadence 180・増加するdistanceをNotifyする。
  if (millis() - lastUpdate >= 1000)
  {
    lastUpdate = millis();
    totalDistance += 30; // en: +3.0 m at 1/10 m units / ja: 1/10 m単位で+3.0 m

    uint8_t measurement[10];
    measurement[0] = 0x03; // en: stride + distance present, walking / ja: stride + distance あり、歩行
    measurement[1] = 0x00; // en: speed 768 = 3.0 m/s / ja: speed 768 = 3.0 m/s
    measurement[2] = 0x03;
    measurement[3] = 180;  // cadence
    measurement[4] = 125;  // en: stride length 1.25 m / ja: stride length 1.25 m
    measurement[5] = 0x00;
    measurement[6] = static_cast<uint8_t>(totalDistance & 0xFF);
    measurement[7] = static_cast<uint8_t>((totalDistance >> 8) & 0xFF);
    measurement[8] = static_cast<uint8_t>((totalDistance >> 16) & 0xFF);
    measurement[9] = static_cast<uint8_t>((totalDistance >> 24) & 0xFF);
    ble.gattServer().setValue(RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID, measurement, sizeof(measurement));
    ble.gattServer().notify(RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID, measurement, sizeof(measurement));
  }

  ble.update();
  delay(1);
}
