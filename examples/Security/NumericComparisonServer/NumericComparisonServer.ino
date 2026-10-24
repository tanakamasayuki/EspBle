// en: Pairing by confirming the 6 digits shown on both sides (peripheral)
// ja: 両側に出た6桁の一致を確認するPairing（Peripheral側）
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "9f78d830-802e-43e7-9003-706173736b79";
static constexpr const char *CHARACTERISTIC_UUID = "9f78d831-802e-43e7-9003-706173736b79";

EspBle ble;

EspBleGattService service;
EspBleGattCharacteristic characteristic;
bool awaitingConfirmation = false;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;
  valueConfig.authenticatedRead = true;

  auto &gattServer = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  service = gattServer.addService(SERVICE_UUID);
  characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(characteristic, String("MITM protected value"));

  EspBleConfig config;
  config.deviceName = "EspBle NumCmp";
  // en: Pairing secures the link; bonding saves trust for later connections (see README).
  // ja: Pairingでリンクを保護し、Bondingで次回の接続用に信頼情報を保存します（README参照）。
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  // en: display + yes/no. Numeric Comparison is chosen only when both sides
  //     declare this and both require MITM; anything else falls back.
  // ja: 表示＋Yes/No。Numeric Comparisonは両側がこれを申告し、両側がMITMを
  //     要求したときにだけ選ばれる。それ以外の組み合わせでは別の方式になる。
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
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &) {
    awaitingConfirmation = false;
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle NumCmp");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();

  Serial.println("Send 'c' while disconnected to clear all bonds.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if ((command == 'y' || command == 'n') && awaitingConfirmation)
    {
      // en: Pairing is blocked waiting for this answer and gives up after 30 s.
      // ja: Pairingはこの答えを待って止まっており、30秒で打ち切られる。
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
