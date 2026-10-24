// en: Read the Feature bit field and write a delete-bond op code
// ja: Feature bit fieldのReadとbond削除op codeのWrite
#include <EspBle.h>

static constexpr const char *BMS_SERVICE_UUID = "181e";
static constexpr const char *BOND_MANAGEMENT_CONTROL_POINT_UUID = "2aa4";
static constexpr const char *BOND_MANAGEMENT_FEATURE_UUID = "2aa5";

EspBle ble;
bool requested = false;

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
    requested = false;
    ble.readCharacteristic(connection.id, BMS_SERVICE_UUID, BOND_MANAGEMENT_FEATURE_UUID);
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (!result.characteristicUuid.equalsIgnoreCase(BOND_MANAGEMENT_FEATURE_UUID) ||
        !result.success || result.value.length() < 3)
      return;
    uint32_t feature = 0;
    for (int i = 2; i >= 0; --i)
      feature = (feature << 8) | static_cast<uint8_t>(result.value[i]);
    Serial.printf("Bond Management Feature: 0x%06x\n", static_cast<unsigned>(feature));
    if (!requested)
    {
      requested = true;
      // en: Op code 0x03 = Delete bond of requesting device (LE transport only).
      // ja: op code 0x03 = Delete bond of requesting device（LE transportのみ）。
      const uint8_t request[1] = {0x03};
      ble.writeCharacteristic(
        result.connectionId, BMS_SERVICE_UUID, BOND_MANAGEMENT_CONTROL_POINT_UUID, request, sizeof(request), true);
    }
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    if (result.characteristicUuid.equalsIgnoreCase(BOND_MANAGEMENT_CONTROL_POINT_UUID) && result.success)
      Serial.println("Delete-bond op code sent");
  });

  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (result.connectable && result.advertisesService(BMS_SERVICE_UUID))
    {
      ble.scanner().stop();
      ble.connect(result);
    }
  });
  ble.scanner().start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
