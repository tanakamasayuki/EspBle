// en: CustomClient - read a Custom HID device's arbitrary Report Descriptor and
//     drive its reports using the generic GATT client. Pairs with the
//     Hid/CustomDevice example. A HID device exposes several Report
//     characteristics that share UUID 0x2A4D, so every attribute here is named by
//     its distinct attribute HANDLE. Each report's role comes from its Report
//     Reference descriptor (0x2908, report ID + type), which is read BY HANDLE:
//     every Report Reference is 0x2908 under a 0x2A4D characteristic, so a
//     service/characteristic/descriptor UUID triple cannot pick one out.
// ja: CustomClient - 汎用GATT clientでCustom HIDデバイスの任意Report Descriptorを読み、
//     Reportを駆動する。Hid/CustomDevice とペア。HIDデバイスは同一UUID 0x2A4Dの
//     Report characteristicを複数持つため、対象はすべて個別のattribute handleで指定する。
//     各Reportの役割はReport Reference descriptor（0x2908、report ID＋type）から読み、
//     その指定もhandleで行う。Report Referenceはどれも「0x2A4Dの下の0x2908」なので、
//     Service/Characteristic/Descriptor UUIDの組では選び分けられない。
#include <EspBle.h>

static constexpr const char *HID_SERVICE_UUID = "1812";
static constexpr const char *REPORT_UUID = "2a4d";
static constexpr const char *REPORT_REFERENCE_UUID = "2908";

// Report Reference type byte (HID over GATT).
static constexpr uint8_t ReportTypeInput = 1;
static constexpr uint8_t ReportTypeOutput = 2;

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
    // Ask each Report characteristic what it is, by reading its own Report
    // Reference. A descriptor belongs to one characteristic, and the link is the
    // owning value handle: discoveredDescriptor() reports it as
    // characteristicHandle. The UUID pair cannot do it here, because every
    // characteristic is 0x2A4D and every descriptor is 0x2908.
    const size_t characteristicCount =
      ble.discoveredCharacteristicCount(result.connectionId, HID_SERVICE_UUID);
    const size_t descriptorCount =
      ble.discoveredDescriptorCount(result.connectionId, HID_SERVICE_UUID);
    size_t requested = 0;
    for (size_t index = 0; index < characteristicCount; ++index)
    {
      EspBleGattCharacteristicInfo info;
      if (!ble.discoveredCharacteristic(result.connectionId, index, info, HID_SERVICE_UUID))
        continue;
      if (!uuidIs(info.characteristicUuid, REPORT_UUID)) continue;
      for (size_t d = 0; d < descriptorCount; ++d)
      {
        EspBleGattDescriptorInfo descriptor;
        if (!ble.discoveredDescriptor(result.connectionId, d, descriptor, HID_SERVICE_UUID))
          continue;
        if (descriptor.characteristicHandle != info.handle) continue;
        if (!uuidIs(descriptor.descriptorUuid, REPORT_REFERENCE_UUID)) continue;
        // Read the descriptor BY HANDLE. Calls are queued automatically and run
        // in order, so all of them can be issued here.
        if (ble.readDescriptor(result.connectionId, descriptor.handle)) ++requested;
        break;
      }
    }
    Serial.printf("Reading %u Report Reference descriptors\n",
      static_cast<unsigned>(requested));
  });
  ble.onDescriptorRead([](const EspBleGattResult &result) {
    if (!result.success || result.value.length() < 2) return;
    // result.handle is the characteristic that owns the descriptor, so the role
    // read out of the descriptor lands on the right Report characteristic.
    const uint8_t reportType = static_cast<uint8_t>(result.value[1]);
    if (reportType == ReportTypeInput)
    {
      inputHandle = result.handle;
      Serial.printf("Input report: id=%u handle=%u\n",
        static_cast<uint8_t>(result.value[0]), inputHandle);
      ble.subscribe(result.connectionId, inputHandle, true); // subscribe by handle
    }
    else if (reportType == ReportTypeOutput)
    {
      outputHandle = result.handle;
      Serial.printf("Output report: id=%u handle=%u\n",
        static_cast<uint8_t>(result.value[0]), outputHandle);
    }
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
