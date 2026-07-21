// phone_alert_status DUT: EspBle GATT client for the Phone Alert Status Service.
// It reads Alert Status, subscribes to Ringer Setting notifications, reads the
// initial Ringer Setting, and drives the Ringer Control Point (Write Without
// Response) to set and cancel Silent Mode, decoding the resulting notifications.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *PASS_SERVICE_UUID = "180e";
static constexpr const char *ALERT_STATUS_UUID = "2a3f";
static constexpr const char *RINGER_SETTING_UUID = "2a41";
static constexpr const char *RINGER_CONTROL_POINT_UUID = "2a40";

EspBle ble;
TaskHandle_t loopTask = nullptr;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

static bool writeRingerControl(uint8_t command)
{
  // Ringer Control Point is Write Without Response: withResponse = false.
  return ble.writeCharacteristic(
    connectionId, PASS_SERVICE_UUID, RINGER_CONTROL_POINT_UUID, &command, sizeof(command), false);
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
    const bool accepted = ble.readCharacteristic(connection.id, PASS_SERVICE_UUID, ALERT_STATUS_UUID);
    Serial.println(accepted ? "ALERT_STATUS_READ_REQUESTED" : "ALERT_STATUS_READ_REQUEST_FAILED");
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(ALERT_STATUS_UUID))
    {
      const bool valid = result.success && result.value.length() == 1;
      Serial.printf("ALERT_STATUS_READ valid=%u status=%02x context=%s\n",
        valid ? 1 : 0, valid ? static_cast<uint8_t>(result.value[0]) : 0, contextName());
      if (valid)
      {
        const bool accepted = ble.subscribe(result.connectionId, PASS_SERVICE_UUID, RINGER_SETTING_UUID);
        Serial.println(accepted ? "RINGER_SUBSCRIBE_REQUESTED" : "RINGER_SUBSCRIBE_REQUEST_FAILED");
      }
    }
    else if (result.characteristicUuid.equalsIgnoreCase(RINGER_SETTING_UUID))
    {
      const bool valid = result.success && result.value.length() == 1;
      Serial.printf("RINGER_SETTING valid=%u value=%u context=%s\n",
        valid ? 1 : 0, valid ? static_cast<uint8_t>(result.value[0]) : 0, contextName());
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("RINGER_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("RINGER_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(RINGER_SETTING_UUID))
      return;
    const bool valid = notification.value.length() == 1;
    Serial.printf("RINGER_NOTIFY valid=%u value=%u context=%s\n",
      valid ? 1 : 0, valid ? static_cast<uint8_t>(notification.value[0]) : 0, contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(PASS_SERVICE_UUID))
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
    else if (command == 'r' && connectionId != 0)
    {
      const bool accepted = ble.readCharacteristic(connectionId, PASS_SERVICE_UUID, RINGER_SETTING_UUID);
      Serial.println(accepted ? "RINGER_READ_REQUESTED" : "RINGER_READ_REQUEST_FAILED");
    }
    else if (command == 'q' && connectionId != 0)
    {
      // Set Silent Mode (command 1).
      Serial.println(writeRingerControl(1) ? "SILENT_REQUESTED" : "SILENT_REQUEST_FAILED");
    }
    else if (command == 'c' && connectionId != 0)
    {
      // Cancel Silent Mode (command 3).
      Serial.println(writeRingerControl(3) ? "CANCEL_REQUESTED" : "CANCEL_REQUEST_FAILED");
    }
    else if (command == 'u' && connectionId != 0)
    {
      const bool accepted = ble.unsubscribe(connectionId, PASS_SERVICE_UUID, RINGER_SETTING_UUID);
      Serial.println(accepted ? "RINGER_UNSUBSCRIBE_REQUESTED" : "RINGER_UNSUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
