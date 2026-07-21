// pulse_oximeter peer_device: EspBle GATT server for the standard Pulse
// Oximeter Service (PLX). PLX Spot-Check Measurement (0x2A5E) is an indication
// carrying SpO2 and pulse rate as IEEE-11073 16-bit SFLOATs; PLX Features
// (0x2A60) is readable.
#include <EspBle.h>
#include <EspBleMedicalFloat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *PLX_SERVICE_UUID = "1822";
static constexpr const char *PLX_SPOT_CHECK_UUID = "2a5e";
static constexpr const char *PLX_FEATURES_UUID = "2a60";

EspBle ble;
TaskHandle_t loopTask = nullptr;
const uint8_t features[2] = {0x03, 0x00};

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig spotCheckConfig;
  spotCheckConfig.indicatable = true;
  EspBleGattCharacteristicConfig featuresConfig;
  featuresConfig.readable = true;
  auto &server = ble.gattServer();
  if (!server.addService(PLX_SERVICE_UUID) ||
      !server.addCharacteristic(PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID, spotCheckConfig) ||
      !server.addCharacteristic(PLX_SERVICE_UUID, PLX_FEATURES_UUID, featuresConfig) ||
      !server.setValue(PLX_SERVICE_UUID, PLX_FEATURES_UUID, features, sizeof(features)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("PLX_SUBSCRIPTION indications=%u context=%s\n",
      subscription.indications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("PLX_SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0, static_cast<unsigned>(result.value.length()), contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle PLX Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle PLX Peer");
  ble.advertising().addServiceUuid(PLX_SERVICE_UUID);
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
      // flags 0x00 (no optional fields), SpO2 98 % and pulse rate 60 bpm as
      // SFLOATs (exponent 0).
      uint8_t measurement[5];
      measurement[0] = 0x00;
      espBleWriteMedicalSFloatLE(&measurement[1], 98, 0);
      espBleWriteMedicalSFloatLE(&measurement[3], 60, 0);
      const bool stored = ble.gattServer().setValue(
        PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID, measurement, sizeof(measurement));
      const bool indicated = ble.gattServer().indicate(
        PLX_SERVICE_UUID, PLX_SPOT_CHECK_UUID, measurement, sizeof(measurement));
      Serial.printf("PLX_UPDATED stored=%u indicated=%u\n", stored ? 1 : 0, indicated ? 1 : 0);
    }
  }
  ble.update();
  delay(1);
}
