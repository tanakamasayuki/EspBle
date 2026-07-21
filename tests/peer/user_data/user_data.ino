// user_data DUT: EspBle GATT client for the User Data Service. It reads Age,
// subscribes to Database Change Increment notifications, writes First Name and a
// new Age (with response), and re-reads Age to confirm the server stored it.
// This exercises the client-write -> server onWritten -> server notify path.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *USER_DATA_SERVICE_UUID = "181c";
static constexpr const char *AGE_UUID = "2a80";
static constexpr const char *FIRST_NAME_UUID = "2a8a";
static constexpr const char *DB_CHANGE_INCREMENT_UUID = "2a99";

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
    const bool accepted = ble.subscribe(
      connection.id, USER_DATA_SERVICE_UUID, DB_CHANGE_INCREMENT_UUID);
    Serial.println(accepted ? "DBCI_SUBSCRIBE_REQUESTED" : "DBCI_SUBSCRIBE_REQUEST_FAILED");
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("DBCI_SUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onUnsubscribed([](const EspBleGattResult &result) {
    Serial.printf("DBCI_UNSUBSCRIBED success=%u context=%s\n", result.success ? 1 : 0, contextName());
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(AGE_UUID))
      return;
    const bool valid = result.success && result.value.length() == 1;
    Serial.printf("AGE_READ valid=%u value=%u context=%s\n",
      valid ? 1 : 0, valid ? static_cast<uint8_t>(result.value[0]) : 0, contextName());
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    const char *which = result.characteristicUuid.equalsIgnoreCase(AGE_UUID) ? "age" : "name";
    Serial.printf("WRITTEN char=%s success=%u context=%s\n", which, result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(DB_CHANGE_INCREMENT_UUID))
      return;
    const String &value = notification.value;
    const bool valid = value.length() == 4;
    uint32_t increment = 0;
    if (valid)
    {
      for (int i = 3; i >= 0; --i)
        increment = (increment << 8) | static_cast<uint8_t>(value[i]);
    }
    Serial.printf("DBCI_NOTIFY valid=%u value=%u context=%s\n",
      valid ? 1 : 0, static_cast<unsigned>(increment), contextName());
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(USER_DATA_SERVICE_UUID))
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
    else if (command == 'a' && connectionId != 0)
    {
      const bool accepted = ble.readCharacteristic(connectionId, USER_DATA_SERVICE_UUID, AGE_UUID);
      Serial.println(accepted ? "AGE_READ_REQUESTED" : "AGE_READ_REQUEST_FAILED");
    }
    else if (command == 'n' && connectionId != 0)
    {
      const uint8_t name[3] = {'A', 'd', 'a'};
      const bool accepted = ble.writeCharacteristic(
        connectionId, USER_DATA_SERVICE_UUID, FIRST_NAME_UUID, name, sizeof(name), true);
      Serial.println(accepted ? "NAME_WRITE_REQUESTED" : "NAME_WRITE_REQUEST_FAILED");
    }
    else if (command == 'w' && connectionId != 0)
    {
      const uint8_t age = 42;
      const bool accepted = ble.writeCharacteristic(
        connectionId, USER_DATA_SERVICE_UUID, AGE_UUID, &age, sizeof(age), true);
      Serial.println(accepted ? "AGE_WRITE_REQUESTED" : "AGE_WRITE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
