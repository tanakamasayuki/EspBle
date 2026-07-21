// running_speed_cadence peer_device: EspBle GATT server for the standard
// Running Speed and Cadence Service. RSC Measurement (0x2A53) is a notification
// with a mixed-width layout (uint16 speed, uint8 cadence, optional uint16 stride
// length and uint32 total distance); RSC Feature (0x2A54) and Sensor Location
// (0x2A5D) are readable.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *RSC_SERVICE_UUID = "1814";
static constexpr const char *RSC_MEASUREMENT_UUID = "2a53";
static constexpr const char *RSC_FEATURE_UUID = "2a54";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const uint8_t feature[2] = {0x03, 0x00}; // Stride Length + Total Distance supported
const uint8_t sensorLocation = 2;        // In Shoe

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig readConfig;
  readConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(RSC_SERVICE_UUID) ||
      !server.addCharacteristic(RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(RSC_SERVICE_UUID, RSC_FEATURE_UUID, readConfig) ||
      !server.addCharacteristic(RSC_SERVICE_UUID, SENSOR_LOCATION_UUID, readConfig) ||
      !server.setValue(RSC_SERVICE_UUID, RSC_FEATURE_UUID, feature, sizeof(feature)) ||
      !server.setValue(RSC_SERVICE_UUID, SENSOR_LOCATION_UUID, &sensorLocation, 1))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("RSC_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("RSC_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle RSC Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle RSC Peer");
  ble.advertising().addServiceUuid(RSC_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == '?')
    {
      Serial.printf("ADVERTISING %u\n", ble.advertising().isAdvertising() ? 1 : 0);
    }
    else if (command == 'h')
    {
      // flags 0x03 (stride length + total distance present, walking). Speed 768
      // (3.0 m/s at 1/256), cadence 180, stride 125 (1.25 m), distance 10000
      // (1000.0 m at 1/10).
      uint8_t measurement[10];
      measurement[0] = 0x03;
      measurement[1] = 0x00; // speed uint16 LE (768 = 0x0300)
      measurement[2] = 0x03;
      measurement[3] = 180;  // cadence uint8
      measurement[4] = 125;  // stride length uint16 LE (125)
      measurement[5] = 0x00;
      measurement[6] = 0x10; // total distance uint32 LE (10000 = 0x00002710)
      measurement[7] = 0x27;
      measurement[8] = 0x00;
      measurement[9] = 0x00;
      const bool stored = ble.gattServer().setValue(
        RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID, measurement, sizeof(measurement));
      const bool notified = ble.gattServer().notify(
        RSC_SERVICE_UUID, RSC_MEASUREMENT_UUID, measurement, sizeof(measurement));
      Serial.printf("RSC_UPDATED stored=%u notified=%u\n", stored ? 1 : 0, notified ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
