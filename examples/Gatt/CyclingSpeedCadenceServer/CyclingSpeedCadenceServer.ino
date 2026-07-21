// en: CyclingSpeedCadenceServer - standard Cycling Speed and Cadence Service
//     (0x1816). CSC Measurement (0x2A5B) is notified with cumulative wheel/crank
//     revolutions and event times; CSC Feature (0x2A5C) and Sensor Location
//     (0x2A5D) are readable.
// ja: CyclingSpeedCadenceServer - 標準Cycling Speed and Cadence Service
//     （0x1816）。CSC Measurement（0x2A5B）を累積wheel/crank回転数とイベント時刻で
//     Notifyし、CSC Feature（0x2A5C）とSensor Location（0x2A5D）はReadできる。
#include <EspBle.h>

static constexpr const char *CSC_SERVICE_UUID = "1816";
static constexpr const char *CSC_MEASUREMENT_UUID = "2a5b";
static constexpr const char *CSC_FEATURE_UUID = "2a5c";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;
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
  server.addService(CSC_SERVICE_UUID);
  server.addCharacteristic(CSC_SERVICE_UUID, CSC_MEASUREMENT_UUID, measurementConfig);
  server.addCharacteristic(CSC_SERVICE_UUID, CSC_FEATURE_UUID, readConfig);
  server.addCharacteristic(CSC_SERVICE_UUID, SENSOR_LOCATION_UUID, readConfig);
  server.setValue(CSC_SERVICE_UUID, CSC_FEATURE_UUID, feature, sizeof(feature));
  server.setValue(CSC_SERVICE_UUID, SENSOR_LOCATION_UUID, &sensorLocation, 1);

  EspBleConfig config;
  config.deviceName = "EspBle CSC";
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
    measurement[0] = 0x03; // en: wheel + crank present / ja: wheel + crank あり
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
    ble.gattServer().setValue(CSC_SERVICE_UUID, CSC_MEASUREMENT_UUID, measurement, sizeof(measurement));
    ble.gattServer().notify(CSC_SERVICE_UUID, CSC_MEASUREMENT_UUID, measurement, sizeof(measurement));
  }

  ble.update();
  delay(1);
}
