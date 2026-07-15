#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "866d5d30-cd84-442d-9003-6d7475746573";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "866d5d31-cd84-442d-9003-6d7475746573";

EspBle ble;
TaskHandle_t loopTask = nullptr;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  characteristicConfig.notifiable = true;
  auto &gattServer = ble.gattServer();
  gattServer.addService(TEST_SERVICE_UUID);
  gattServer.addCharacteristic(
    TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, characteristicConfig);
  gattServer.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf(
      "SUBSCRIPTION notifications=%u context=%s\n",
      subscription.notifications ? 1 : 0,
      callbackContext());
  });
  gattServer.onSent([](const EspBleGattSendResult &result) {
    Serial.printf(
      "SENT success=%u length=%u context=%s\n",
      result.success ? 1 : 0,
      static_cast<unsigned>(result.value.length()),
      callbackContext());
  });

  EspBleConfig config;
  config.deviceName = "EspBle MTU Peer";
  config.preferredMtu = 128;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onMtuChanged([](const EspBleMtuChanged &event) {
    EspBleConnection storedConnection;
    const bool stored = ble.connection(event.connection.id, storedConnection);
    Serial.printf(
      "PERIPHERAL_MTU previous=%u mtu=%u stored=%u payload=%u context=%s\n",
      event.previousMtu,
      event.connection.mtu,
      stored && storedConnection.mtu == event.connection.mtu ? 1 : 0,
      static_cast<unsigned>(event.connection.maximumNotificationPayload()),
      callbackContext());
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle MTU Peer");
  advertising.addServiceUuid(TEST_SERVICE_UUID);
  advertising.start();
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
      String payload;
      payload.reserve(125);
      for (size_t index = 0; index < 125; ++index) payload += 'a';
      Serial.printf(
        "%s length=%u\n",
        ble.gattServer().notify(TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, payload)
          ? "MAX_NOTIFY_REQUESTED"
          : "MAX_NOTIFY_REQUEST_FAILED",
        static_cast<unsigned>(payload.length()));
    }
    else if (command == 'o')
    {
      String payload;
      payload.reserve(126);
      for (size_t index = 0; index < 126; ++index) payload += 'b';
      const bool accepted = ble.gattServer().notify(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, payload);
      Serial.printf(
        "%s error=%s\n",
        accepted ? "OVERSIZE_ACCEPTED" : "OVERSIZE_REJECTED",
        ble.lastErrorName());
    }
  }

  ble.update();
  delay(1);
}
