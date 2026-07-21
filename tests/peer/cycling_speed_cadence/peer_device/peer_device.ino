// cycling_speed_cadence peer_device: EspBle GATT server for the standard
// Cycling Speed and Cadence Service. CSC Measurement (0x2A5B) is a notification
// with cumulative wheel/crank revolutions and event times; CSC Feature (0x2A5C)
// and Sensor Location (0x2A5D) are readable.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *CSC_SERVICE_UUID = "1816";
static constexpr const char *CSC_MEASUREMENT_UUID = "2a5b";
static constexpr const char *CSC_FEATURE_UUID = "2a5c";
static constexpr const char *SENSOR_LOCATION_UUID = "2a5d";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const uint8_t feature[2] = {0x03, 0x00};   // Wheel + Crank Revolution supported
const uint8_t sensorLocation = 12;         // Rear Hub

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static void appendU16(uint8_t *out, uint16_t value)
{
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
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
  if (!server.addService(CSC_SERVICE_UUID) ||
      !server.addCharacteristic(CSC_SERVICE_UUID, CSC_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(CSC_SERVICE_UUID, CSC_FEATURE_UUID, readConfig) ||
      !server.addCharacteristic(CSC_SERVICE_UUID, SENSOR_LOCATION_UUID, readConfig) ||
      !server.setValue(CSC_SERVICE_UUID, CSC_FEATURE_UUID, feature, sizeof(feature)) ||
      !server.setValue(CSC_SERVICE_UUID, SENSOR_LOCATION_UUID, &sensorLocation, 1))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("CSC_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("CSC_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle CSC Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle CSC Peer");
  ble.advertising().addServiceUuid(CSC_SERVICE_UUID);
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
      // flags 0x03 (wheel + crank present), 100 wheel revs, last wheel event
      // 2048 (2 s at 1/1024), 50 crank revs, last crank event 1024 (1 s).
      uint8_t measurement[11];
      measurement[0] = 0x03;
      measurement[1] = 100; // cumulative wheel revolutions (uint32 LE)
      measurement[2] = 0;
      measurement[3] = 0;
      measurement[4] = 0;
      appendU16(&measurement[5], 2048);
      appendU16(&measurement[7], 50);
      appendU16(&measurement[9], 1024);
      const bool stored = ble.gattServer().setValue(
        CSC_SERVICE_UUID, CSC_MEASUREMENT_UUID, measurement, sizeof(measurement));
      const bool notified = ble.gattServer().notify(
        CSC_SERVICE_UUID, CSC_MEASUREMENT_UUID, measurement, sizeof(measurement));
      Serial.printf("CSC_UPDATED stored=%u notified=%u\n", stored ? 1 : 0, notified ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
