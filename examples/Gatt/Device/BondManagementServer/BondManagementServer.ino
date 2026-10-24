// en: Bond Management Feature read, Control Point delete-bond op codes
// ja: Bond Management Feature Read、Control Pointのbond削除op code
#include <EspBle.h>

static constexpr const char *BMS_SERVICE_UUID = "181e";
static constexpr const char *BOND_MANAGEMENT_CONTROL_POINT_UUID = "2aa4";
static constexpr const char *BOND_MANAGEMENT_FEATURE_UUID = "2aa5";

EspBle ble;
EspBleGattService bmsServiceService;
EspBleGattCharacteristic bondManagementControlPointCharacteristic;
EspBleGattCharacteristic bondManagementFeatureCharacteristic;
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
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  bmsServiceService = server.addService(BMS_SERVICE_UUID);
  bondManagementControlPointCharacteristic = server.addCharacteristic(bmsServiceService, BOND_MANAGEMENT_CONTROL_POINT_UUID, controlConfig);
  bondManagementFeatureCharacteristic = server.addCharacteristic(bmsServiceService, BOND_MANAGEMENT_FEATURE_UUID, featureConfig);
  server.setValue(bondManagementFeatureCharacteristic, feature, sizeof(feature));

  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
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

  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    connectedPeer = connection.peerAddress;
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &connection) {
    if (deleteBondOnDisconnect)
    {
      deleteBondOnDisconnect = false;
      deleteBondForPeer(connection.peerAddress);
    }
  });

  EspBleConfig config;
  config.deviceName = "EspBle Bond Management";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
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
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
