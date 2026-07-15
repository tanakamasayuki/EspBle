#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "866d5d30-cd84-442d-9003-6d7475746573";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "866d5d31-cd84-442d-9003-6d7475746573";

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
  config.deviceName = "EspBle MTU Central";
  config.preferredMtu = 185;
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf(
      "CENTRAL_CONNECTED id=%u mtu=%u payload=%u\n",
      static_cast<unsigned>(connection.id),
      connection.mtu,
      static_cast<unsigned>(connection.maximumNotificationPayload()));
  });
  ble.onMtuChanged([](const EspBleMtuChanged &event) {
    EspBleConnection storedConnection;
    const bool stored = ble.connection(event.connection.id, storedConnection);
    Serial.printf(
      "CENTRAL_MTU previous=%u mtu=%u stored=%u payload=%u context=%s\n",
      event.previousMtu,
      event.connection.mtu,
      stored && storedConnection.mtu == event.connection.mtu ? 1 : 0,
      static_cast<unsigned>(event.connection.maximumNotificationPayload()),
      callbackContext());
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, callbackContext());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf(
      "NOTIFICATION length=%u indication=%u context=%s\n",
      static_cast<unsigned>(notification.value.length()),
      notification.indication ? 1 : 0,
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
  }

  ble.update();
  delay(1);
}
