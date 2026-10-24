// en: The central half of the same
// ja: 同上のCentral側
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "9f78d830-802e-43e7-9003-706173736b79";
static constexpr const char *CHARACTERISTIC_UUID = "9f78d831-802e-43e7-9003-706173736b79";

EspBle ble;
bool connectionRequested = false;
bool awaitingConfirmation = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle NumCmp Client";
  // en: Pairing secures the link; bonding saves trust for later connections (see README).
  // ja: Pairingでリンクを保護し、Bondingで次回の接続用に信頼情報を保存します（README参照）。
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  config.security.ioCapability = EspBleSecurityIoCapability::DisplayYesNo;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onNumericComparison([](const EspBlePasskeyDisplayed &event) {
    awaitingConfirmation = true;
    Serial.printf(
      "Does the peer show %06u? Send 'y' to accept, 'n' to reject.\n",
      static_cast<unsigned>(event.passkey));
  });
  // en: Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
  // ja: セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    awaitingConfirmation = false;
    Serial.printf(
      "Security %s: encrypted=%u authenticated=%u bonded=%u\n",
      event.success ? "established" : "failed",
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0);
    if (event.success)
    {
      ble.discoverCharacteristic(event.connection.id, SERVICE_UUID, CHARACTERISTIC_UUID);
    }
  });
  ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
    if (result.success)
    {
      ble.readCharacteristic(result.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID);
    }
  });
  // en: Read results arrive asynchronously; check success and length before decoding.
  // ja: Read結果は後から届くので、成功と長さを確認してからデコードします。
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    if (result.success)
    {
      Serial.printf("Protected value: %s\n", result.value.c_str());
    }
    else
    {
      Serial.printf("Read failed: %s\n", result.detail.c_str());
    }
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &) {
    connectionRequested = false;
    awaitingConfirmation = false;
  });
  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &scanResult) {
    if (connectionRequested || !scanResult.advertisesService(SERVICE_UUID))
    {
      return;
    }
    ble.scanner().stop();
    connectionRequested = ble.connect(scanResult);
  });

  EspBleScanConfig scanConfig;
  scanConfig.active = true;
  ble.scanner().start(scanConfig);

  Serial.println("Send 'c' while disconnected to clear all bonds.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if ((command == 'y' || command == 'n') && awaitingConfirmation)
    {
      Serial.printf(
        "Answer %s: %s\n",
        command == 'y' ? "accept" : "reject",
        ble.confirmNumericComparison(command == 'y') ? "sent" : ble.lastErrorName());
    }
    else if (command == 'c')
    {
      Serial.printf(
        "Clear bonds: %s, remaining=%u\n",
        ble.deleteAllBonds() ? "success" : ble.lastErrorName(),
        static_cast<unsigned>(ble.bondCount()));
    }
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
