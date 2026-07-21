// hid_custom DUT: EspBle generic GATT client that validates a Custom HID device
// built with an arbitrary Report Descriptor. It reads the Report Map (0x2A4B)
// and checks its length and vendor-defined signature, then subscribes to the
// input Report (0x2A4D) and decodes a custom 2-byte report (dial delta + buttons).
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *HID_SERVICE_UUID = "1812";
static constexpr const char *REPORT_MAP_UUID = "2a4b";
static constexpr const char *REPORT_UUID = "2a4d";

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
    Serial.printf("CONNECTED id=%u\n", static_cast<unsigned>(connection.id));
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(REPORT_MAP_UUID)) return;
    const unsigned length = result.value.length();
    const unsigned b0 = length > 0 ? static_cast<uint8_t>(result.value[0]) : 0;
    const unsigned b1 = length > 1 ? static_cast<uint8_t>(result.value[1]) : 0;
    const unsigned b2 = length > 2 ? static_cast<uint8_t>(result.value[2]) : 0;
    Serial.printf("REPORT_MAP success=%u length=%u sig=%u,%u,%u context=%s\n",
      result.success ? 1 : 0, length, b0, b1, b2, contextName());
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(REPORT_UUID)) return;
    Serial.printf("INPUT_SUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(REPORT_UUID)) return;
    const bool valid = notification.value.length() == 2;
    const int delta = valid ? static_cast<int8_t>(notification.value[0]) : 0;
    const unsigned buttons = valid ? static_cast<uint8_t>(notification.value[1]) : 0;
    Serial.printf("INPUT_REPORT length=%u delta=%d buttons=%u context=%s\n",
      static_cast<unsigned>(notification.value.length()), delta, buttons, contextName());
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(HID_SERVICE_UUID)) return;
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
    else if (command == 'm' && connectionId != 0)
    {
      Serial.println(ble.readCharacteristic(connectionId, HID_SERVICE_UUID, REPORT_MAP_UUID)
        ? "READ_REQUESTED" : "READ_REQUEST_FAILED");
    }
    else if (command == 'S' && connectionId != 0)
    {
      Serial.println(ble.subscribe(connectionId, HID_SERVICE_UUID, REPORT_UUID, true)
        ? "SUBSCRIBE_REQUESTED" : "SUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
