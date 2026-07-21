// continuous_glucose_monitoring peer_device: EspBle GATT server for the CGM
// Service. CGM Feature (0x2AA8) is a readable value protected by an E2E-CRC;
// CGM Measurement (0x2AA7) is notified with an SFLOAT glucose concentration, a
// time offset, and an appended E2E-CRC. The CRC is computed with the shared
// EspBleCgmCrc.h codec (CRC-16/MCRF4XX), the same codec the client verifies with.
#include <EspBle.h>
#include <EspBleCgmCrc.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *CGM_SERVICE_UUID = "181f";
static constexpr const char *CGM_MEASUREMENT_UUID = "2aa7";
static constexpr const char *CGM_FEATURE_UUID = "2aa8";

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static void buildFeature(uint8_t *out)
{
  // Feature uint24: bit 12 = E2E-CRC supported (0x001000).
  out[0] = 0x00;
  out[1] = 0x10;
  out[2] = 0x00;
  // Type/Sample Location: Type = Capillary Whole blood (1), Location = Finger (1).
  out[3] = 0x11;
  espBleCgmAppendCrc(out, 4); // out[4], out[5] = E2E-CRC
}

static void buildMeasurement(uint8_t *out)
{
  out[0] = 8;    // Size (total octets including CRC)
  out[1] = 0x00; // Flags: no optional fields
  espBleWriteMedicalSFloatLE(&out[2], 100, 0); // Glucose concentration SFLOAT = 100
  out[4] = 0x05; // Time Offset uint16 LE = 5 minutes
  out[5] = 0x00;
  espBleCgmAppendCrc(out, 6); // out[6], out[7] = E2E-CRC
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig measurementConfig;
  measurementConfig.notifiable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  uint8_t feature[6];
  buildFeature(feature);
  if (!server.addService(CGM_SERVICE_UUID) ||
      !server.addCharacteristic(CGM_SERVICE_UUID, CGM_MEASUREMENT_UUID, measurementConfig) ||
      !server.addCharacteristic(CGM_SERVICE_UUID, CGM_FEATURE_UUID, featureConfig) ||
      !server.setValue(CGM_SERVICE_UUID, CGM_FEATURE_UUID, feature, sizeof(feature)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("CGM_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("CGM_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle CGM Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle CGM Peer");
  ble.advertising().addServiceUuid(CGM_SERVICE_UUID);
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
    else if (command == 'm')
    {
      uint8_t measurement[8];
      buildMeasurement(measurement);
      const bool stored = ble.gattServer().setValue(
        CGM_SERVICE_UUID, CGM_MEASUREMENT_UUID, measurement, sizeof(measurement));
      const bool notified = ble.gattServer().notify(
        CGM_SERVICE_UUID, CGM_MEASUREMENT_UUID, measurement, sizeof(measurement));
      Serial.printf("CGM_UPDATED stored=%u notified=%u\n", stored ? 1 : 0, notified ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
