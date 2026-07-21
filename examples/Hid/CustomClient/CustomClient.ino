// en: CustomClient - read a Custom HID device's arbitrary Report Descriptor and
//     drive its reports using the generic GATT client. Pairs with the
//     Hid/CustomDevice example. A HID device exposes several Report
//     characteristics that share UUID 0x2A4D, so this example discovers the
//     service, resolves each report to its distinct attribute HANDLE, subscribes
//     to the input report by handle, and writes the output report by handle.
// ja: CustomClient - 汎用GATT clientでCustom HIDデバイスの任意Report Descriptorを読み、
//     Reportを駆動する。Hid/CustomDevice とペア。HIDデバイスは同一UUID 0x2A4Dの
//     Report characteristicを複数持つため、serviceをdiscoverして各Reportを個別の
//     attribute handleへ解決し、入力Reportをhandleで購読、出力Reportをhandleで書き込む。
#include <EspBle.h>

static constexpr const char *HID_SERVICE_UUID = "1812";
static constexpr const char *REPORT_UUID = "2a4d";

EspBle ble;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;
uint16_t inputHandle = 0;
uint16_t outputHandle = 0;

// Discovered UUIDs are in full 128-bit form (0000XXXX-...); match either way.
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
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    ble.discoverServices(connection.id);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    connectionId = 0;
    connectionRequested = false;
    inputHandle = outputHandle = 0;
  });
  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    if (!result.success) return;
    // Resolve the two same-UUID Report characteristics by role via their handles.
    const size_t count = ble.discoveredCharacteristicCount(result.connectionId, HID_SERVICE_UUID);
    for (size_t index = 0; index < count; ++index)
    {
      EspBleGattCharacteristicInfo info;
      if (!ble.discoveredCharacteristic(result.connectionId, index, info, HID_SERVICE_UUID))
        continue;
      if (!uuidIs(info.characteristicUuid, REPORT_UUID)) continue;
      if (info.notifiable) inputHandle = info.handle;
      else if (info.writable) outputHandle = info.handle;
    }
    Serial.printf("Resolved input handle=%u, output handle=%u\n", inputHandle, outputHandle);
    if (inputHandle != 0)
      ble.subscribe(result.connectionId, inputHandle, true); // subscribe by handle
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.handle != inputHandle || notification.value.length() < 2) return;
    Serial.printf("Input report: dial delta=%d buttons=%u\n",
      static_cast<int8_t>(notification.value[0]),
      static_cast<uint8_t>(notification.value[1]));
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(HID_SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
  });

  EspBleScanConfig scan;
  scan.active = true;
  ble.scanner().start(scan);
  Serial.println("Scanning for a Custom HID device. Send 'o' to write the output LED report.");
}

void loop()
{
  if (Serial.available() > 0 && Serial.read() == 'o' && outputHandle != 0)
  {
    const uint8_t leds = 0x02; // write the output report by handle
    ble.writeCharacteristic(connectionId, outputHandle, &leds, sizeof(leds), true);
  }
  ble.update();
  delay(1);
}
