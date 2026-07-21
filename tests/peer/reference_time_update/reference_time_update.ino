// reference_time_update DUT: EspBle GATT client for the Reference Time Update
// Service. It reads the 2-byte Time Update State, writes the Time Update Control
// Point (Write Without Response) to request then cancel a reference update, and
// re-reads the state each time to confirm the server-side transitions.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *RTUS_SERVICE_UUID = "1806";
static constexpr const char *TIME_UPDATE_CONTROL_POINT_UUID = "2a16";
static constexpr const char *TIME_UPDATE_STATE_UUID = "2a17";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static bool writeControlPoint(uint8_t command)
{
  // Time Update Control Point is Write Without Response: withResponse = false.
  return ble.writeCharacteristic(
    connectionId, RTUS_SERVICE_UUID, TIME_UPDATE_CONTROL_POINT_UUID, &command, sizeof(command), false);
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
    const bool accepted = ble.readCharacteristic(connection.id, RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID);
    Serial.println(accepted ? "STATE_READ_REQUESTED" : "STATE_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(TIME_UPDATE_STATE_UUID))
      return;
    const bool valid = result.success && result.value.length() == 2;
    const uint8_t current = valid ? static_cast<uint8_t>(result.value[0]) : 0xFF;
    const uint8_t resultCode = valid ? static_cast<uint8_t>(result.value[1]) : 0xFF;
    Serial.printf("STATE valid=%u current=%u result=%u context=%s\n",
      valid ? 1 : 0, current, resultCode, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(RTUS_SERVICE_UUID))
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
    else if (command == 'g' && connectionId != 0)
    {
      // Get Reference Update (command 1).
      Serial.println(writeControlPoint(1) ? "GET_REQUESTED" : "GET_REQUEST_FAILED");
    }
    else if (command == 'c' && connectionId != 0)
    {
      // Cancel Reference Update (command 2).
      Serial.println(writeControlPoint(2) ? "CANCEL_REQUESTED" : "CANCEL_REQUEST_FAILED");
    }
    else if (command == 'r' && connectionId != 0)
    {
      const bool accepted = ble.readCharacteristic(connectionId, RTUS_SERVICE_UUID, TIME_UPDATE_STATE_UUID);
      Serial.println(accepted ? "STATE_READ_REQUESTED" : "STATE_READ_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
