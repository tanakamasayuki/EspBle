#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "71756360-5fa4-43bc-9003-6e6f74696679";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "71756361-5fa4-43bc-9003-6e6f74696679";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *callbackContext()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleConfig config;
  config.deviceName = "EspBle Subscription Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("CENTRAL_CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf(
      "SUBSCRIBED success=%u notifications=%u indications=%u context=%s\n",
      result.success ? 1 : 0,
      result.subscribedToNotifications ? 1 : 0,
      result.subscribedToIndications ? 1 : 0,
      callbackContext());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf(
      "UNSUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0,
      callbackContext());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf(
      "RECEIVED id=%u indication=%u value=%s context=%s\n",
      static_cast<unsigned>(notification.connectionId),
      notification.indication ? 1 : 0,
      notification.value.c_str(),
      callbackContext());
  });
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(TEST_SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
    Serial.println(connectionRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 's' && !connectionRequested)
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'n' && connectionId != 0)
    {
      Serial.println(
        ble.subscribe(connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, true)
          ? "SUBSCRIBE_NOTIFY_REQUESTED"
          : "SUBSCRIBE_NOTIFY_REQUEST_FAILED");
    }
    else if (command == 'i' && connectionId != 0)
    {
      Serial.println(
        ble.subscribe(connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, false)
          ? "SUBSCRIBE_INDICATE_REQUESTED"
          : "SUBSCRIBE_INDICATE_REQUEST_FAILED");
    }
    else if (command == 'u' && connectionId != 0)
    {
      Serial.println(
        ble.unsubscribe(connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID)
          ? "UNSUBSCRIBE_REQUESTED"
          : "UNSUBSCRIBE_REQUEST_FAILED");
    }
  }

  ble.update();
  delay(1);
}
