// body_composition peer_device: EspBle GATT server for the standard Body
// Composition Service. Body Composition Measurement (0x2A9C) is an indication
// carrying uint16 flags, the mandatory Body Fat Percentage (0.1 %/LSB), and
// optional fields; Body Composition Feature (0x2A9B) is a readable uint32.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *BODY_COMPOSITION_SERVICE_UUID = "181b";
static constexpr const char *BODY_COMPOSITION_MEASUREMENT_UUID = "2a9c";
static constexpr const char *BODY_COMPOSITION_FEATURE_UUID = "2a9b";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const uint8_t feature[4] = {0x00, 0x02, 0x00, 0x00}; // bit 9 = Weight Measurement Supported

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
  if (!server.addService(BODY_COMPOSITION_SERVICE_UUID) ||
      !server.addCharacteristic(
        BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(
        BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_FEATURE_UUID, featureConfig) ||
      !server.setValue(BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_FEATURE_UUID, feature, sizeof(feature)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("BC_SUBSCRIPTION indications=%u context=%s\n",
      subscription.indications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("BC_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle BodyComp Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle BodyComp Peer");
  ble.advertising().addServiceUuid(BODY_COMPOSITION_SERVICE_UUID);
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
      // flags = 0x0400 (SI units, Weight present). Body Fat Percentage 27.5 % at
      // 0.1 %/LSB is raw 275; Weight 70.000 kg at 0.005 kg/LSB is raw 14000. All
      // little-endian.
      const uint16_t flags = 0x0400;
      const uint16_t fatRaw = 275;
      const uint16_t weightRaw = 14000;
      uint8_t measurement[6];
      measurement[0] = static_cast<uint8_t>(flags & 0xFF);
      measurement[1] = static_cast<uint8_t>((flags >> 8) & 0xFF);
      measurement[2] = static_cast<uint8_t>(fatRaw & 0xFF);
      measurement[3] = static_cast<uint8_t>((fatRaw >> 8) & 0xFF);
      measurement[4] = static_cast<uint8_t>(weightRaw & 0xFF);
      measurement[5] = static_cast<uint8_t>((weightRaw >> 8) & 0xFF);
      const bool stored = ble.gattServer().setValue(
        BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID, measurement, sizeof(measurement));
      const bool indicated = ble.gattServer().indicate(
        BODY_COMPOSITION_SERVICE_UUID, BODY_COMPOSITION_MEASUREMENT_UUID, measurement, sizeof(measurement));
      Serial.printf("BC_UPDATED stored=%u indicated=%u\n", stored ? 1 : 0, indicated ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
