// weight_scale peer_device: EspBle GATT server for the standard Weight Scale
// Service. Weight Measurement (0x2A9D) is an indication carrying a uint16 weight
// at 0.005 kg resolution; Weight Scale Feature (0x2A9E) is a readable uint32.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *WEIGHT_SCALE_SERVICE_UUID = "181d";
static constexpr const char *WEIGHT_MEASUREMENT_UUID = "2a9d";
static constexpr const char *WEIGHT_SCALE_FEATURE_UUID = "2a9e";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const uint8_t feature[4] = {0x08, 0x00, 0x00, 0x00}; // weight-resolution field present

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
  measurementConfig.indicatable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(WEIGHT_SCALE_SERVICE_UUID) ||
      !server.addCharacteristic(
        WEIGHT_SCALE_SERVICE_UUID, WEIGHT_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(
        WEIGHT_SCALE_SERVICE_UUID, WEIGHT_SCALE_FEATURE_UUID, featureConfig) ||
      !server.setValue(WEIGHT_SCALE_SERVICE_UUID, WEIGHT_SCALE_FEATURE_UUID, feature, sizeof(feature)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("WS_SUBSCRIPTION indications=%u context=%s\n",
      subscription.indications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("WS_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Scale Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Scale Peer");
  ble.advertising().addServiceUuid(WEIGHT_SCALE_SERVICE_UUID);
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
      // flags = 0x00 (SI kg, no optional fields). 70.000 kg at 0.005 kg/LSB is
      // raw 14000, little-endian.
      const uint16_t raw = 14000;
      uint8_t measurement[3];
      measurement[0] = 0x00;
      measurement[1] = static_cast<uint8_t>(raw & 0xFF);
      measurement[2] = static_cast<uint8_t>((raw >> 8) & 0xFF);
      const bool stored = ble.gattServer().setValue(
        WEIGHT_SCALE_SERVICE_UUID, WEIGHT_MEASUREMENT_UUID, measurement, sizeof(measurement));
      const bool indicated = ble.gattServer().indicate(
        WEIGHT_SCALE_SERVICE_UUID, WEIGHT_MEASUREMENT_UUID, measurement, sizeof(measurement));
      Serial.printf("WS_UPDATED stored=%u indicated=%u\n", stored ? 1 : 0, indicated ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
