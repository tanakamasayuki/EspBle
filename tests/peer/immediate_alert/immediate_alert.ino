// immediate_alert DUT: EspBle GATT client for the Immediate Alert Service (the
// Find Me profile locator role). Alert Level (0x2A06) is Write Without Response
// only, so the client writes High Alert then No Alert without expecting a
// response; the server observes each via onWritten.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *IMMEDIATE_ALERT_SERVICE_UUID = "1802";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static bool writeAlertLevel(uint8_t level)
{
  // Write Without Response: withResponse = false.
  return ble.writeCharacteristic(
    connectionId, IMMEDIATE_ALERT_SERVICE_UUID, ALERT_LEVEL_UUID, &level, sizeof(level), false);
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
    Serial.printf("CONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(IMMEDIATE_ALERT_SERVICE_UUID))
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
    else if (command == 'h' && connectionId != 0)
    {
      Serial.println(writeAlertLevel(2) ? "HIGH_ALERT_REQUESTED" : "HIGH_ALERT_REQUEST_FAILED");
    }
    else if (command == 'n' && connectionId != 0)
    {
      Serial.println(writeAlertLevel(0) ? "NO_ALERT_REQUESTED" : "NO_ALERT_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
