#include <EspBle.h>

static constexpr const char *HEART_RATE_SERVICE_UUID = "180d";
static constexpr const char *HEART_RATE_MEASUREMENT_UUID = "2a37";
static constexpr const char *BODY_SENSOR_LOCATION_UUID = "2a38";

EspBle ble;
uint8_t heartRate = 70;
uint8_t measurement[] = {0x10, 70, 0x00, 0x04}; // 8-bit bpm + one RR interval.
const uint8_t bodySensorLocation = 1; // Chest.

static void publishMeasurement()
{
  measurement[1] = heartRate;
  auto &server = ble.gattServer();
  server.setValue(HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID,
    measurement, sizeof(measurement));
  const bool notified = server.notify(
    HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID,
    measurement, sizeof(measurement));
  Serial.printf("Heart rate: %u bpm (notification accepted: %u)\n",
    heartRate, notified ? 1 : 0);
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig locationConfig;
  locationConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(HEART_RATE_SERVICE_UUID) ||
      !server.addCharacteristic(
        HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(
        HEART_RATE_SERVICE_UUID, BODY_SENSOR_LOCATION_UUID, locationConfig) ||
      !server.setValue(HEART_RATE_SERVICE_UUID, HEART_RATE_MEASUREMENT_UUID,
        measurement, sizeof(measurement)) ||
      !server.setValue(HEART_RATE_SERVICE_UUID, BODY_SENSOR_LOCATION_UUID,
        &bodySensorLocation, 1))
  {
    Serial.printf("Heart Rate configuration failed: %s\n",
      ble.lastErrorDetail().c_str());
    return;
  }

  EspBleConfig config;
  config.deviceName = "EspBle Heart Rate";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Heart Rate");
  ble.advertising().addServiceUuid(HEART_RATE_SERVICE_UUID);
  ble.advertising().start();
  Serial.println("Send '+' or '-' to change the heart rate and notify subscribers.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '+' && heartRate < 250)
    {
      ++heartRate;
      publishMeasurement();
    }
    else if (command == '-' && heartRate > 1)
    {
      --heartRate;
      publishMeasurement();
    }
  }
  ble.update();
  delay(1);
}
