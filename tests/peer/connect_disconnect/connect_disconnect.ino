#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *TEST_SERVICE_UUID = "5266f727-49d7-4eaf-a6f1-636f6e6e6563";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId activeConnectionId = 0;
bool connectionRequested = false;

static const char *roleName(EspBleRole role)
{
  return role == EspBleRole::Central ? "CENTRAL" : "PERIPHERAL";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();

  EspBleConfig config;
  config.deviceName = "EspBle Central";
  if (!ble.begin(config))
  {
    Serial.printf("BLE_INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    activeConnectionId = connection.id;
    Serial.printf(
      "CENTRAL_CONNECTED id=%u role=%s count=%u context=%s\n",
      static_cast<unsigned>(connection.id),
      roleName(connection.localRole),
      static_cast<unsigned>(ble.connectionCount()),
      xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack");
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    Serial.printf(
      "CENTRAL_DISCONNECTED id=%u role=%s count=%u context=%s\n",
      static_cast<unsigned>(connection.id),
      roleName(connection.localRole),
      static_cast<unsigned>(ble.connectionCount()),
      xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack");
    activeConnectionId = 0;
  });
  ble.onConnectionFailed([](const EspBleConnectionFailure &failure) {
    Serial.printf("CONNECT_FAILED %s %s\n", failure.peerAddress.c_str(), failure.detail.c_str());
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
    else if (command == 'd' && activeConnectionId != 0)
    {
      Serial.println(ble.disconnect(activeConnectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }

  ble.update();
  delay(1);
}
