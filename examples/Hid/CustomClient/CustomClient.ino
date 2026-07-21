// en: CustomClient - read a Custom HID device's arbitrary Report Descriptor and
//     receive its input reports using the generic GATT client. Pairs with the
//     Hid/CustomDevice example. Because a HID device exposes several Report
//     characteristics that share UUID 0x2A4D, this client subscribes to the first
//     one (the device registers its input report first).
// ja: CustomClient - 汎用GATT clientでCustom HIDデバイスの任意Report Descriptorを読み、
//     入力Reportを受け取る。Hid/CustomDevice とペア。HIDデバイスは同一UUID 0x2A4Dの
//     Report characteristicを複数持つため、この例は最初の1つ（デバイスが最初に登録した
//     入力Report）を購読する。
#include <EspBle.h>

static constexpr const char *HID_SERVICE_UUID = "1812";
static constexpr const char *REPORT_MAP_UUID = "2a4b";
static constexpr const char *REPORT_UUID = "2a4d";

EspBle ble;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;

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
    // en: Read the Report Map, then subscribe to the input report.
    // ja: Report Mapを読み、入力Reportを購読する。
    ble.readCharacteristic(connection.id, HID_SERVICE_UUID, REPORT_MAP_UUID);
    ble.subscribe(connection.id, HID_SERVICE_UUID, REPORT_UUID, true);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    connectionId = 0;
    connectionRequested = false;
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(REPORT_MAP_UUID)) return;
    Serial.printf("Report Map: %u bytes\n", result.value.length());
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (!notification.characteristicUuid.equalsIgnoreCase(REPORT_UUID)) return;
    if (notification.value.length() < 2) return;
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
  Serial.println("Scanning for a Custom HID device...");
}

void loop()
{
  ble.update();
  delay(1);
}
