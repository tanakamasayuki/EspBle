#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "71756361-5fa4-43bc-9003-6e6f74696679";

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

  auto &gattServer = ble.gattServer();
  EspBleGattCharacteristicConfig characteristicConfig;
  characteristicConfig.readable = true;
  characteristicConfig.notifiable = true;
  characteristicConfig.indicatable = true;
  if (!gattServer.addService(TEST_SERVICE_UUID) ||
      !gattServer.addCharacteristic(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, characteristicConfig))
  {
    Serial.printf("GATT_CONFIG_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  gattServer.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
    Serial.printf(
      "SUBSCRIPTION id=%u notifications=%u indications=%u context=%s\n",
      static_cast<unsigned>(subscription.connectionId),
      subscription.notifications ? 1 : 0,
      subscription.indications ? 1 : 0,
      callbackContext());
  });
  gattServer.onSent([](const EspBleGattSendResult &result) {
    Serial.printf(
      "SENT indication=%u success=%u value=%s detail=%s context=%s\n",
      result.indication ? 1 : 0,
      result.success ? 1 : 0,
      result.value.c_str(),
      result.detail.c_str(),
      callbackContext());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Subscription Peer";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Subscription Peer");
  advertising.addServiceUuid(TEST_SERVICE_UUID);
  if (!advertising.start())
  {
    Serial.printf("ADVERTISING_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
  }
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
    else if (command == 'n')
    {
      Serial.println(
        ble.gattServer().notify(
          TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("notify-value"))
          ? "NOTIFY_REQUESTED"
          : "NOTIFY_REQUEST_FAILED");
    }
    else if (command == 'i')
    {
      Serial.println(
        ble.gattServer().indicate(
          TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("indicate-value"))
          ? "INDICATE_REQUESTED"
          : "INDICATE_REQUEST_FAILED");
    }
  }

  ble.update();
  delay(1);
}
