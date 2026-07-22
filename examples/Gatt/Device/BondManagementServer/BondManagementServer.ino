// en: BondManagementServer - standard Bond Management Service (0x181E). Bond
//     Management Feature (0x2AA5) is a readable uint24 bit field of supported
//     operations; the Bond Management Control Point (0x2AA4) is writable and
//     receives op codes in onWritten. On "Delete bond of requesting device"
//     (0x03), the server removes that peer's bond after it disconnects.
// ja: BondManagementServer - 標準Bond Management Service（0x181E）。Bond
//     Management Feature（0x2AA5）は対応操作のread可能なuint24 bit field、Bond
//     Management Control Point（0x2AA4）はwritableでop codeをonWrittenで受け取る。
//     「Delete bond of requesting device」（0x03）では、切断後にそのpeerのbondを
//     削除する。
#include <EspBle.h>

static constexpr const char *BMS_SERVICE_UUID = "181e";
static constexpr const char *BOND_MANAGEMENT_CONTROL_POINT_UUID = "2aa4";
static constexpr const char *BOND_MANAGEMENT_FEATURE_UUID = "2aa5";

EspBle ble;
// en: bit 0 = Delete bond of requesting device (LE); bit 4 = Delete all bonds (LE + BR/EDR).
// ja: bit 0 = Delete bond of requesting device（LE）、bit 4 = 全bond削除（LE + BR/EDR）。
const uint8_t feature[3] = {0x11, 0x00, 0x00};
String connectedPeer;
bool deleteBondOnDisconnect = false;

static void deleteBondForPeer(const String &peerAddress)
{
  const size_t count = ble.bondCount();
  for (size_t i = 0; i < count; ++i)
  {
    EspBleBond bond;
    if (ble.bond(i, bond) && bond.peerAddress.equalsIgnoreCase(peerAddress))
    {
      if (ble.deleteBond(bond))
        Serial.println("Bond deleted");
      return;
    }
  }
}

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig controlConfig;
  controlConfig.writable = true;
  EspBleGattCharacteristicConfig featureConfig;
  featureConfig.readable = true;
  auto &server = ble.gattServer();
  server.addService(BMS_SERVICE_UUID);
  server.addCharacteristic(BMS_SERVICE_UUID, BOND_MANAGEMENT_CONTROL_POINT_UUID, controlConfig);
  server.addCharacteristic(BMS_SERVICE_UUID, BOND_MANAGEMENT_FEATURE_UUID, featureConfig);
  server.setValue(BMS_SERVICE_UUID, BOND_MANAGEMENT_FEATURE_UUID, feature, sizeof(feature));

  server.onWritten([](const EspBleGattWrite &write) {
    if (!write.characteristicUuid.equalsIgnoreCase(BOND_MANAGEMENT_CONTROL_POINT_UUID) || write.value.length() < 1)
      return;
    const uint8_t opCode = static_cast<uint8_t>(write.value[0]);
    Serial.printf("Bond Management op code: %u\n", opCode);
    // en: 0x03 = Delete bond of requesting device (LE). Defer the removal until
    //     the client disconnects so it does not disrupt the active link.
    // ja: 0x03 = Delete bond of requesting device（LE）。アクティブなリンクを
    //     壊さないよう、削除はClient切断後まで遅延させる。
    if (opCode == 0x03)
      deleteBondOnDisconnect = true;
  });

  ble.onConnected([](const EspBleConnection &connection) {
    connectedPeer = connection.peerAddress;
  });
  ble.onDisconnected([](const EspBleConnection &connection) {
    if (deleteBondOnDisconnect)
    {
      deleteBondOnDisconnect = false;
      deleteBondForPeer(connection.peerAddress);
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Bond Management";
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  ble.advertising().addServiceUuid(BMS_SERVICE_UUID);
  ble.advertising().start();
}

void loop()
{
  ble.update();
  delay(1);
}
