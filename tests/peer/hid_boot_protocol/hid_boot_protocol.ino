// hid_boot_protocol DUT: EspBle generic GATT client that drives the HID over
// GATT Boot Protocol on a keyboard peripheral. It reads the Protocol Mode
// characteristic (0x2A4E), switches the device to Boot Protocol Mode, subscribes
// to the Boot Keyboard Input Report (0x2A22), receives an 8-byte boot report,
// and writes the Boot Keyboard Output Report (0x2A32) LED state.
#include <EspBle.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr const char *HID_SERVICE_UUID = "1812";
static constexpr const char *PROTOCOL_MODE_UUID = "2a4e";
static constexpr const char *BOOT_KEYBOARD_INPUT_UUID = "2a22";
static constexpr const char *BOOT_KEYBOARD_OUTPUT_UUID = "2a32";

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
    if (!result.characteristicUuid.equalsIgnoreCase(PROTOCOL_MODE_UUID)) return;
    const unsigned mode = result.value.length() >= 1
      ? static_cast<uint8_t>(result.value[0]) : 255u;
    Serial.printf("PROTOCOL_MODE_READ success=%u mode=%u context=%s\n",
      result.success ? 1 : 0, mode, contextName());
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(PROTOCOL_MODE_UUID))
      Serial.printf("PROTOCOL_MODE_WRITTEN success=%u context=%s\n",
        result.success ? 1 : 0, contextName());
    else if (result.characteristicUuid.equalsIgnoreCase(BOOT_KEYBOARD_OUTPUT_UUID))
      Serial.printf("BOOT_OUTPUT_WRITTEN success=%u context=%s\n",
        result.success ? 1 : 0, contextName());
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(BOOT_KEYBOARD_INPUT_UUID)) return;
    Serial.printf("BOOT_SUBSCRIBED success=%u context=%s\n",
      result.success ? 1 : 0, contextName());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(BOOT_KEYBOARD_INPUT_UUID)) return;
    const bool validLength = notification.value.length() == 8;
    const unsigned modifiers = validLength ? static_cast<uint8_t>(notification.value[0]) : 0;
    const unsigned key = validLength ? static_cast<uint8_t>(notification.value[2]) : 0;
    Serial.printf("BOOT_INPUT length=%u modifiers=%u key=%u context=%s\n",
      static_cast<unsigned>(notification.value.length()), modifiers, key, contextName());
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
    else if (command == 'p' && connectionId != 0)
    {
      const bool ok = ble.readCharacteristic(connectionId, HID_SERVICE_UUID, PROTOCOL_MODE_UUID);
      Serial.println(ok ? "READ_REQUESTED" : "READ_REQUEST_FAILED");
    }
    else if (command == 'B' && connectionId != 0)
    {
      const bool ok = ble.subscribe(connectionId, HID_SERVICE_UUID, BOOT_KEYBOARD_INPUT_UUID, true);
      Serial.println(ok ? "BOOT_SUBSCRIBE_REQUESTED" : "BOOT_SUBSCRIBE_REQUEST_FAILED");
    }
    else if (command == 'b' && connectionId != 0)
    {
      const uint8_t boot = EspBleHidKeyboard::BootProtocolMode;
      // Protocol Mode is Write Without Response.
      const bool ok = ble.writeCharacteristic(
        connectionId, HID_SERVICE_UUID, PROTOCOL_MODE_UUID, &boot, sizeof(boot), false);
      Serial.println(ok ? "PROTOCOL_MODE_WRITE_REQUESTED" : "PROTOCOL_MODE_WRITE_REQUEST_FAILED");
    }
    else if (command == 'o' && connectionId != 0)
    {
      const uint8_t leds = 0x02; // Caps Lock LED
      const bool ok = ble.writeCharacteristic(
        connectionId, HID_SERVICE_UUID, BOOT_KEYBOARD_OUTPUT_UUID, &leds, sizeof(leds), true);
      Serial.println(ok ? "BOOT_OUTPUT_WRITE_REQUESTED" : "BOOT_OUTPUT_WRITE_REQUEST_FAILED");
    }
    else if (command == 'd' && connectionId != 0)
    {
      Serial.println(ble.disconnect(connectionId) ? "DISCONNECT_REQUESTED" : "DISCONNECT_REQUEST_FAILED");
    }
  }
  ble.update();
  delay(1);
}
