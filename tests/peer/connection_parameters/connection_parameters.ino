// connection_parameters DUT: EspBle GATT client that connects to the peer,
// reports the initial connection interval from onConnected, requests a
// connection parameter update via updateConnectionParameters(), and reports the
// negotiated parameters delivered to onConnectionParametersUpdated().
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *SERVICE_UUID = "1815"; // advertised marker

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  loopTask = xTaskGetCurrentTaskHandle();
  if (!ble.begin())
  {
    Serial.printf("INIT_FAILED %s %s\n", ble.lastErrorName(), ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.printf("CONNECTED id=%u interval=%u latency=%u timeout=%u context=%s\n",
      static_cast<unsigned>(connection.id), connection.connectionInterval,
      connection.peripheralLatency, connection.supervisionTimeout, contextName());
  });
  ble.onConnectionParametersUpdated([](const EspBleConnection &connection) {
    Serial.printf("PARAMS_UPDATED interval=%u latency=%u timeout=%u context=%s\n",
      connection.connectionInterval, connection.peripheralLatency,
      connection.supervisionTimeout, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(SERVICE_UUID))
      return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
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
      EspBleScanConfig scan;
      scan.active = true;
      Serial.println(ble.scanner().start(scan) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'p' && connectionId != 0)
    {
      // Request a distinctly different interval: 80 * 1.25 ms = 100 ms, no
      // latency, 2 s supervision timeout (200 * 10 ms).
      const bool accepted = ble.updateConnectionParameters(connectionId, 80, 80, 0, 200);
      Serial.println(accepted ? "UPDATE_REQUESTED" : "UPDATE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
