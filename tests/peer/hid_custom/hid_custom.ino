// hid_custom DUT: EspBle generic GATT client that validates a Custom HID device
// built with an arbitrary Report Descriptor, AND the handle-based client
// operations. The device exposes three Report characteristics that share UUID
// 0x2A4D (a notifiable input, a writable output and a writable feature). The
// client discovers the service, resolves each report by its attribute handle,
// subscribes to the input by handle, decodes a custom 2-byte report, and writes
// the output and the feature by handle.
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
uint16_t inputHandle = 0;
uint16_t outputHandle = 0;
uint16_t featureHandle = 0;

static const char *contextName()
{
  return xTaskGetCurrentTaskHandle() == loopTask ? "loop" : "stack";
}

// Discovered UUIDs come back in full 128-bit form (0000XXXX-...); match the
// 16-bit short UUID either way.
static bool uuidIs(const String &uuid, const char *shortUuid)
{
  String lower = uuid;
  lower.toLowerCase();
  String needle = shortUuid;
  needle.toLowerCase();
  return lower == needle || lower.indexOf(needle) == 4;
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
    ble.discoverServices(connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    connectionId = 0;
    connectionRequested = false;
    inputHandle = 0;
    outputHandle = 0;
    featureHandle = 0;
    Serial.printf("DISCONNECTED id=%u context=%s\n",
      static_cast<unsigned>(connection.id), contextName());
  });
  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    if (!result.success)
    {
      Serial.println("DISCOVER_FAILED");
      return;
    }
    // Resolve the three 0x2A4D Report characteristics by role using their
    // handles. Output and Feature are both writable, so the discriminator is
    // Write Without Response: an Output report is a stream and carries it, a
    // Feature report is configuration and is always written with a response.
    const size_t count = ble.discoveredCharacteristicCount(result.connectionId, HID_SERVICE_UUID);
    for (size_t index = 0; index < count; ++index)
    {
      EspBleGattCharacteristicInfo info;
      if (!ble.discoveredCharacteristic(result.connectionId, index, info, HID_SERVICE_UUID))
        continue;
      if (!uuidIs(info.characteristicUuid, REPORT_UUID)) continue;
      if (info.notifiable) inputHandle = info.handle;
      else if (info.writableWithoutResponse) outputHandle = info.handle;
      else if (info.writable) featureHandle = info.handle;
    }
    Serial.printf("REPORTS_RESOLVED input=%u output=%u feature=%u distinct=%u\n",
      inputHandle, outputHandle, featureHandle,
      (inputHandle != 0 && outputHandle != 0 && featureHandle != 0 &&
       inputHandle != outputHandle && outputHandle != featureHandle &&
       inputHandle != featureHandle) ? 1 : 0);
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(REPORT_MAP_UUID)) return;
    Serial.printf("REPORT_MAP success=%u length=%u\n",
      result.success ? 1 : 0, result.value.length());
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    // The result handle says which of the two writable reports this was.
    Serial.printf("%s success=%u handle=%u context=%s\n",
      result.handle == featureHandle ? "FEATURE_WRITTEN" : "OUTPUT_WRITTEN",
      result.success ? 1 : 0, result.handle, contextName());
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf("INPUT_SUBSCRIBED success=%u handle=%u context=%s\n",
      result.success ? 1 : 0, result.handle, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.handle != inputHandle || notification.value.length() != 2) return;
    Serial.printf("INPUT_REPORT handle=%u delta=%d buttons=%u context=%s\n",
      notification.handle,
      static_cast<int8_t>(notification.value[0]),
      static_cast<uint8_t>(notification.value[1]), contextName());
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
    else if (command == 'S' && inputHandle != 0)
    {
      // Subscribe to the input report by handle (not UUID).
      Serial.println(ble.subscribe(connectionId, inputHandle, true)
        ? "SUBSCRIBE_REQUESTED" : "SUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'o' && outputHandle != 0)
    {
      // Write the output report by handle: the other 0x2A4D characteristic.
      const uint8_t leds = 0x02;
      Serial.println(ble.writeCharacteristic(connectionId, outputHandle, &leds, sizeof(leds), true)
        ? "OUTPUT_WRITE_REQUESTED" : "OUTPUT_WRITE_REQUEST_FAILED");
    }
    else if (command == 'f' && featureHandle != 0)
    {
      // Write the feature report by handle: two bytes of configuration.
      const uint8_t configuration[2] = {0x5a, 0xa5};
      Serial.println(ble.writeCharacteristic(
        connectionId, featureHandle, configuration, sizeof(configuration), true)
        ? "FEATURE_WRITE_REQUESTED" : "FEATURE_WRITE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
