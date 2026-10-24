// en: Read a Custom HID's Report Map, take each report's role from its Report Reference by handle, and decode the input report
// ja: Custom HIDのReport Mapを読み、Report Referenceをhandle指定で読んで役割を判定し、入力Reportをデコード
#include <EspBle.h>

static constexpr const char *HID_SERVICE_UUID = "1812";
static constexpr const char *REPORT_UUID = "2a4d";
static constexpr const char *REPORT_REFERENCE_UUID = "2908";

// en: Report Reference type byte (HID over GATT).
// ja: HID over GATTのReport Referenceにある種別バイトです。
static constexpr uint8_t ReportTypeInput = 1;
static constexpr uint8_t ReportTypeOutput = 2;

EspBle ble;
EspBleConnectionId connectionId = 0;
bool connectionRequested = false;
uint16_t inputHandle = 0;
uint16_t outputHandle = 0;

// en: Discovered UUIDs are in full 128-bit form (0000XXXX-...); match either way.
// ja: 探索したUUIDは128ビット形式（0000XXXX-...）なので、短い形式とどちらでも照合できるようにします。
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
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin())
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    ble.discoverServices(connection.id);
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &) {
    connectionId = 0;
    connectionRequested = false;
    inputHandle = outputHandle = 0;
  });
  ble.onServicesDiscovered([](const EspBleGattResult &result) {
    if (!result.success) return;
    // en: Ask each Report characteristic what it is, by reading its own Report Reference. A descriptor belongs to one characteristic, and the link is the owning value handle: discoveredDescriptor() reports it as characteristicHandle. The UUID pair cannot do it here, because every characteristic is 0x2A4D and every descriptor is 0x2908.
    // ja: 各Report CharacteristicのReport Referenceを読み、役割を調べます。所有する値のhandleで対応付けます。UUIDが同じReport／Descriptorが複数あるため、UUIDの組だけでは区別できません。
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
        // en: Read the descriptor BY HANDLE. Calls are queued automatically and run in order, so all of them can be issued here.
        // ja: Descriptorをhandleで読みます。要求は自動でキューに並び順番に処理されるので、ここでまとめて発行できます。
        if (ble.readDescriptor(result.connectionId, descriptor.handle)) ++requested;
        break;
      }
    }
    Serial.printf("Reading %u Report Reference descriptors\n",
      static_cast<unsigned>(requested));
  });
  ble.onDescriptorRead([](const EspBleGattResult &result) {
    if (!result.success || result.value.length() < 2) return;
    // en: result.handle is the characteristic that owns the descriptor, so the role read out of the descriptor lands on the right Report characteristic.
    // ja: result.handleはDescriptorを所有するCharacteristicを示すので、調べた役割を正しいReportへ対応付けられます。
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
  // en: Check the characteristic and packet length before interpreting incoming bytes.
  // ja: Characteristicとデータ長を確認してから受信バイトを解釈します。
  ble.onNotification([](const EspBleGattNotification &notification) {
    if (notification.handle != inputHandle || notification.value.length() < 2) return;
    Serial.printf("Input report: dial delta=%d buttons=%u\n",
      static_cast<int8_t>(notification.value[0]),
      static_cast<uint8_t>(notification.value[1]));
  });
  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
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
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
