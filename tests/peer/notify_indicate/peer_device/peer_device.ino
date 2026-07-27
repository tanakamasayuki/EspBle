#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "71756361-5fa4-43bc-9003-6e6f74696679";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId serverConnectionId = 0;

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
      "SENT id=%u indication=%u success=%u value=%s detail=%s context=%s\n",
      static_cast<unsigned>(result.connectionId),
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
  ble.onConnected([](const EspBleConnection &connection) {
    serverConnectionId = connection.id;
    Serial.printf("PERIPHERAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });

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
    else if (command == 'q')
    {
      // Fire three notifies back-to-back without waiting for onSent. Before the
      // send FIFO this rejected every call after the first; now all queue.
      unsigned queued = 0;
      queued += ble.gattServer().notify(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("burst-1")) ? 1 : 0;
      queued += ble.gattServer().notify(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("burst-2")) ? 1 : 0;
      queued += ble.gattServer().notify(
        TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, String("burst-3")) ? 1 : 0;
      Serial.printf("BURST_QUEUED ok=%u\n", queued);
    }
    else if (command == 't')
    {
      // Connection-scoped notify to exactly this connection.
      Serial.println(
        ble.gattServer().notify(
          serverConnectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID,
          String("targeted-value"))
          ? "TARGETED_REQUESTED"
          : "TARGETED_REQUEST_FAILED");
    }
  }

  ble.update();
  delay(1);
}
