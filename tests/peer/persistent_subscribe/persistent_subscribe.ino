#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "70657273-7375-6273-9003-7375627363cc";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "70657273-7375-6273-9003-7375627363cd";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;
unsigned connectCount = 0;
unsigned notifyCount = 0;

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
  config.deviceName = "EspBle Persistent Central";
  // persistentSubscriptions defaults to true; stated here for the test's intent.
  config.persistentSubscriptions = true;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    ++connectCount;
    Serial.printf("CENTRAL_CONNECTED n=%u id=%u\n",
      connectCount, static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("CENTRAL_DISCONNECTED n=%u\n", connectCount);
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf(
      "SUBSCRIBED success=%u notifications=%u connect=%u context=%s\n",
      result.success ? 1 : 0,
      result.subscribedToNotifications ? 1 : 0,
      connectCount,
      callbackContext());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("UNSUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, callbackContext());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    ++notifyCount;
    Serial.printf(
      "RECEIVED value=%s count=%u connect=%u context=%s\n",
      notification.value.c_str(),
      notifyCount,
      connectCount,
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
          ? "SUBSCRIBE_REQUESTED"
          : "SUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'u' && connectionId != 0)
    {
      Serial.println(
        ble.unsubscribe(connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID)
          ? "UNSUBSCRIBE_REQUESTED"
          : "UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'x' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_FAILED");
    }
  }

  ble.update();
  delay(1);
}
