// en: JustWorksServer - a GATT server whose characteristic requires an encrypted link.
//     Pairing uses Just Works (no passkey, LE Secure Connections) + bonding, started
//     automatically on connection. Reading before encryption returns insufficient-
//     encryption, which prompts the OS to pair.
// ja: JustWorksServer - 暗号化されたlinkを要求するCharacteristicを持つGATT Server。
//     PairingはJust Works（passkeyなし、LE Secure Connections）+ Bondingで、接続時に自動開始する。
//     暗号化前にCharacteristicを読むとinsufficient-encryptionエラーになり、OS側のPairingが誘発される。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "be31dd60-5e70-4fd5-9003-736563757265";
static constexpr const char *CHARACTERISTIC_UUID = "be31dd61-5e70-4fd5-9003-736563757265";

EspBle ble;

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
  gattServer.addService(SERVICE_UUID);
  gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(SERVICE_UUID, CHARACTERISTIC_UUID, String("encrypted value"));
  gattServer.onWritten([](const EspBleGattWrite &write) {
    Serial.printf("Encrypted write: %s\n", write.value.c_str());
  });

  EspBleConfig config;
  config.deviceName = "EspBle Secure";
  config.security.enabled = true;       // en: enable security / ja: Security有効化
  config.security.bonding = true;       // en: store keys (auto-encrypt on reconnect) / ja: 鍵を保存（再接続時に自動暗号化）
  config.security.pairOnConnect = true; // en: start pairing on connection / ja: 接続と同時にPairingを開始
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

  ble.update();
  delay(1);
}
