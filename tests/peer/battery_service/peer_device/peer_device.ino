#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *BATTERY_SERVICE_UUID = "180f";
static constexpr const char *BATTERY_LEVEL_UUID = "2a19";

EspBle ble;
TaskHandle_t loopTask = nullptr;
uint8_t batteryLevel = 73;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig levelConfig;
  levelConfig.readable = true;
  levelConfig.notifiable = true;
  auto &server = ble.gattServer();
  if (!server.addService(BATTERY_SERVICE_UUID) ||
      !server.addCharacteristic(BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID, levelConfig) ||
      !server.setValue(BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID, &batteryLevel, 1))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("BATTERY_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("BATTERY_SENT success=%u value=%u context=%s\n",
      result.success ? 1 : 0,
      result.value.length() == 1 ? static_cast<uint8_t>(result.value[0]) : 0,
      contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Battery Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Battery Peer");
  ble.advertising().addServiceUuid(BATTERY_SERVICE_UUID);
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
    else if (command == 'u')
    {
      batteryLevel = 42;
      const bool stored = ble.gattServer().setValue(
        BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID, &batteryLevel, 1);
      const bool notified = ble.gattServer().notify(
        BATTERY_SERVICE_UUID, BATTERY_LEVEL_UUID, &batteryLevel, 1);
      Serial.printf("BATTERY_UPDATED stored=%u notified=%u level=%u\n",
        stored ? 1 : 0, notified ? 1 : 0, batteryLevel);
    }
  }
  ble.update();
  delay(1);
}
