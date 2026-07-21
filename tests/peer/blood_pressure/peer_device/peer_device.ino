// blood_pressure peer_device: EspBle GATT server for the standard Blood
// Pressure Service. Blood Pressure Measurement (0x2A35) is an indication whose
// systolic/diastolic/mean values are IEEE-11073 16-bit SFLOATs; Blood Pressure
// Feature (0x2A49) is a readable 16-bit field.
#include <EspBle.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *BLOOD_PRESSURE_SERVICE_UUID = "1810";
static constexpr const char *BLOOD_PRESSURE_MEASUREMENT_UUID = "2a35";
static constexpr const char *BLOOD_PRESSURE_FEATURE_UUID = "2a49";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const uint8_t feature[2] = {0x03, 0x00}; // Body Movement + Cuff Fit detection

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
  if (!server.addService(BLOOD_PRESSURE_SERVICE_UUID) ||
      !server.addCharacteristic(
        BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(
        BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_FEATURE_UUID, featureConfig) ||
      !server.setValue(BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_FEATURE_UUID, feature, sizeof(feature)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("BP_SUBSCRIPTION indications=%u context=%s\n",
      subscription.indications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("BP_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle BP Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle BP Peer");
  ble.advertising().addServiceUuid(BLOOD_PRESSURE_SERVICE_UUID);
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
      // flags = 0x00 (mmHg, no optional fields), then systolic 120, diastolic
      // 80, mean arterial pressure 93 as SFLOATs (exponent 0).
      uint8_t measurement[7];
      measurement[0] = 0x00;
      espBleWriteMedicalSFloatLE(&measurement[1], 120, 0);
      espBleWriteMedicalSFloatLE(&measurement[3], 80, 0);
      espBleWriteMedicalSFloatLE(&measurement[5], 93, 0);
      const bool stored = ble.gattServer().setValue(
        BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID, measurement, sizeof(measurement));
      const bool indicated = ble.gattServer().indicate(
        BLOOD_PRESSURE_SERVICE_UUID, BLOOD_PRESSURE_MEASUREMENT_UUID, measurement, sizeof(measurement));
      Serial.printf("BP_UPDATED stored=%u indicated=%u\n", stored ? 1 : 0, indicated ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
