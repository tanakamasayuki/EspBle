#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *CURRENT_TIME_SERVICE_UUID = "1805";
static constexpr const char *CURRENT_TIME_UUID = "2a2b";

EspBle ble;
EspBleGattService currentTimeServiceService;
EspBleGattCharacteristic currentTimeCharacteristic;
TaskHandle_t loopTask = nullptr;
uint8_t currentTime[] = {0xea, 0x07, 7, 19, 12, 34, 56, 7, 128, 0x02};

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig timeConfig;
  timeConfig.readable = true;
  timeConfig.notifiable = true;
  auto &server = ble.gattServer();
  if (!(currentTimeServiceService = server.addService(CURRENT_TIME_SERVICE_UUID)).valid() ||
      !(currentTimeCharacteristic = server.addCharacteristic(currentTimeServiceService, CURRENT_TIME_UUID, timeConfig)).valid() ||
      !server.setValue(currentTimeCharacteristic, currentTime, sizeof(currentTime)))
  {
    Serial.printf("CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  server.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf("TIME_SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0, contextName());
  });
  server.onSent([](const EspBleGattSendResult &result) {
    Serial.printf("TIME_SENT success=%u length=%u second=%u context=%s\n",
      result.success ? 1 : 0,
      static_cast<unsigned>(result.value.length()),
      result.value.length() == 10 ? static_cast<uint8_t>(result.value[6]) : 0,
      contextName());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Time Peer";
  if (!ble.begin(config))
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().setName("EspBle Time Peer");
  ble.advertising().addServiceUuid(CURRENT_TIME_SERVICE_UUID);
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
    else if (command == 't')
    {
      currentTime[6] = 57;
      const bool stored = ble.gattServer().setValue(currentTimeCharacteristic, currentTime, sizeof(currentTime));
      const bool notified = ble.gattServer().notify(currentTimeCharacteristic, currentTime, sizeof(currentTime));
      Serial.printf("TIME_UPDATED stored=%u notified=%u second=%u\n",
        stored ? 1 : 0, notified ? 1 : 0, currentTime[6]);
    }
  }
  ble.update();
  delay(1);
}
