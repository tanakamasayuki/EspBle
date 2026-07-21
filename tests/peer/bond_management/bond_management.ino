// bond_management DUT: EspBle GATT client for the Bond Management Service. It
// reads the Bond Management Feature bit field (uint24) and writes the Bond
// Management Control Point op code "Delete bond of requesting device (LE)" with
// response; the server observes the op code in onWritten.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *BMS_SERVICE_UUID = "181e";
static constexpr const char *BOND_MANAGEMENT_CONTROL_POINT_UUID = "2aa4";
static constexpr const char *BOND_MANAGEMENT_FEATURE_UUID = "2aa5";

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
    const bool accepted = ble.readCharacteristic(connection.id, BMS_SERVICE_UUID, BOND_MANAGEMENT_FEATURE_UUID);
    Serial.println(accepted ? "FEATURE_READ_REQUESTED" : "FEATURE_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(BOND_MANAGEMENT_FEATURE_UUID))
      return;
    // Bond Management Feature is a variable-length bit field, minimum 3 bytes.
    const bool valid = result.success && result.value.length() >= 3;
    uint32_t feature = 0;
    if (valid)
    {
      for (int i = 2; i >= 0; --i)
        feature = (feature << 8) | static_cast<uint8_t>(result.value[i]);
    }
    Serial.printf("FEATURE_READ valid=%u value=%06x context=%s\n",
      valid ? 1 : 0, static_cast<unsigned>(feature), contextName());
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(BOND_MANAGEMENT_CONTROL_POINT_UUID))
      Serial.printf("CONTROL_WRITTEN success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(BMS_SERVICE_UUID))
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
    else if (command == 'x' && connectionId != 0)
    {
      // Op Code 0x03 = Delete bond of requesting device (LE transport only).
      const uint8_t request[1] = {0x03};
      const bool accepted = ble.writeCharacteristic(
        connectionId, BMS_SERVICE_UUID, BOND_MANAGEMENT_CONTROL_POINT_UUID, request, sizeof(request), true);
      Serial.println(accepted ? "CONTROL_WRITE_REQUESTED" : "CONTROL_WRITE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
