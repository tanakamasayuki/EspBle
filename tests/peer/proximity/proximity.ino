// proximity DUT: EspBle GATT client for the Proximity profile, which combines
// the Link Loss Service (0x1803, Alert Level read/write) and the Tx Power
// Service (0x1804, Tx Power Level read, signed int8). It reads the Tx Power
// Level, reads the Link Loss Alert Level, writes a new Alert Level with
// response, and re-reads it to confirm the server stored it.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *LINK_LOSS_SERVICE_UUID = "1803";
static constexpr const char *ALERT_LEVEL_UUID = "2a06";
static constexpr const char *TX_POWER_SERVICE_UUID = "1804";
static constexpr const char *TX_POWER_LEVEL_UUID = "2a07";

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
    const bool accepted = ble.readCharacteristic(connection.id, TX_POWER_SERVICE_UUID, TX_POWER_LEVEL_UUID);
    Serial.println(accepted ? "TX_POWER_READ_REQUESTED" : "TX_POWER_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(TX_POWER_LEVEL_UUID))
    {
      const bool valid = result.success && result.value.length() == 1;
      const int power = valid ? static_cast<int8_t>(result.value[0]) : 0;
      Serial.printf("TX_POWER valid=%u value=%d context=%s\n", valid ? 1 : 0, power, contextName());
      if (valid)
      {
        const bool accepted = ble.readCharacteristic(result.connectionId, LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID);
        Serial.println(accepted ? "ALERT_READ_REQUESTED" : "ALERT_READ_REQUEST_FAILED");
      }
    }
    else if (result.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID))
    {
      const bool valid = result.success && result.value.length() == 1;
      Serial.printf("ALERT_LEVEL valid=%u value=%u context=%s\n",
        valid ? 1 : 0, valid ? static_cast<uint8_t>(result.value[0]) : 0, contextName());
    }
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(ALERT_LEVEL_UUID))
      Serial.printf("ALERT_WRITTEN success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(LINK_LOSS_SERVICE_UUID))
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
    else if (command == 'w' && connectionId != 0)
    {
      // High Alert (2), Write With Response.
      const uint8_t level = 2;
      const bool accepted = ble.writeCharacteristic(
        connectionId, LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID, &level, sizeof(level), true);
      Serial.println(accepted ? "ALERT_WRITE_REQUESTED" : "ALERT_WRITE_REQUEST_FAILED");
    }
    else if (command == 'a' && connectionId != 0)
    {
      const bool accepted = ble.readCharacteristic(connectionId, LINK_LOSS_SERVICE_UUID, ALERT_LEVEL_UUID);
      Serial.println(accepted ? "ALERT_READ_REQUESTED" : "ALERT_READ_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
