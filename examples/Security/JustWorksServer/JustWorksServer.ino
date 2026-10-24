// en: Encrypted characteristic with Just Works pairing + bonding
// ja: Just Works Pairing + Bondingと暗号化Characteristic
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "be31dd60-5e70-4fd5-9003-736563757265";
static constexpr const char *CHARACTERISTIC_UUID = "be31dd61-5e70-4fd5-9003-736563757265";

EspBle ble;

EspBleGattService service;
EspBleGattCharacteristic characteristic;
void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;
  valueConfig.writable = true;
  // en: require an encrypted link for read/write (enforced at the ATT layer)
  // ja: Read/Writeに暗号化linkを要求（ATT層で強制）
  valueConfig.encryptedRead = true;
  valueConfig.encryptedWrite = true;

  auto &gattServer = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  service = gattServer.addService(SERVICE_UUID);
  characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(characteristic, String("encrypted value"));
  // en: Handle writes from the peer after update dispatches the event. Validate the UUID, length and command/value before changing application state.
  // ja: updateが配送した相手からの書き込みを処理します。UUID・長さ・コマンドや値を検証してからアプリの状態を変更します。
  gattServer.onWritten([](const EspBleGattWrite &write) {
    Serial.printf("Encrypted write: %s\n", write.value.c_str());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Secure";
  // en: Pairing secures the link; bonding saves trust for later connections (see README).
  // ja: Pairingでリンクを保護し、Bondingで次回の接続用に信頼情報を保存します（README参照）。
  config.security.enabled = true;       // en: enable security / ja: Security有効化
  // en: store keys (auto-encrypt on reconnect)
  // ja: 鍵を保存（再接続時に自動暗号化）
  config.security.bonding = true;
  // en: start pairing on connection
  // ja: 接続と同時にPairingを開始
  config.security.pairOnConnect = true;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Security result. Just Works gives encrypted=1 but authenticated=0 (no MITM).
  // ja: Security確立/失敗の結果。Just Worksは encrypted=1 だが authenticated=0（MITM非認証）。
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "Security %s: encrypted=%u authenticated=%u bonded=%u keySize=%u\n",
      event.success ? "established" : "failed",
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0,
      event.connection.encryptionKeySize);
  });
  // en: Restart advertising on each disconnect to accept reconnections.
  // ja: 切断のたびにadvertisingを再開して再接続を受け付ける。
  ble.onDisconnected([](const EspBleConnection &) {
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Secure");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();

  Serial.println("Send 'c' while disconnected to clear all bonds.");
}

void loop()
{
  // en: On 'c', delete all bonds (allowed only while disconnected).
  // ja: Serialで 'c' を受けたら全Bondを削除（切断中のみ許可される）。
  if (Serial.available() > 0 && Serial.read() == 'c')
  {
    Serial.printf(
      "Clear bonds: %s, remaining=%u\n",
      ble.deleteAllBonds() ? "success" : ble.lastErrorName(),
      static_cast<unsigned>(ble.bondCount()));
  }

  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
