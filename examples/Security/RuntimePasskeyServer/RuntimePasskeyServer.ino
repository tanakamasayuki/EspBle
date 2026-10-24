// en: Display side of a passkey generated per pairing
// ja: Pairingごとに生成されるpasskeyの表示側
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "9f78d820-802e-43e7-9003-706173736b79";
static constexpr const char *CHARACTERISTIC_UUID = "9f78d821-802e-43e7-9003-706173736b79";

EspBle ble;

EspBleGattService service;
EspBleGattCharacteristic characteristic;

void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;
  // en: require a MITM-authenticated link / ja: MITM認証済みlinkを要求
  valueConfig.authenticatedRead = true;

  auto &gattServer = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  service = gattServer.addService(SERVICE_UUID);
  characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(characteristic, String("MITM protected value"));

  EspBleConfig config;
  config.deviceName = "EspBle Runtime Passkey";
  // en: Pairing secures the link; bonding saves trust for later connections (see README).
  // ja: Pairingでリンクを保護し、Bondingで次回の接続用に信頼情報を保存します（README参照）。
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  // en: display side / ja: 表示側
  config.security.ioCapability = EspBleSecurityIoCapability::DisplayOnly;
  // en: staticPasskeyEnabled stays false: that is what makes the passkey random.
  // ja: staticPasskeyEnabled は false のまま。これがpasskeyを毎回変える設定。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Arrives when pairing starts, in update() context. Show it to the user;
  //     the peer has to be told this value out of band (by a human, here).
  // ja: Pairing開始時に update() contextで届く。利用者へ見せる値で、相手へは
  //     BLEの外（ここでは人間）を通じて伝える。
  ble.onPasskeyDisplayed([](const EspBlePasskeyDisplayed &event) {
    Serial.printf("Enter passkey %06u on the peer.\n", static_cast<unsigned>(event.passkey));
  });
  // en: Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
  // ja: セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
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
    ble.advertising().start();
  });

  auto &advertising = ble.advertising();
  advertising.setName("EspBle Runtime Passkey");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();

  Serial.println("Send 'c' while disconnected to clear all bonds.");
}

void loop()
{
  // en: On 'c', delete all bonds (allowed only while disconnected). A bonded peer
  //     reuses the stored key and no passkey is displayed again, so clearing the
  //     bonds on both sides is how to see a second pairing.
  // ja: 'c' で全Bondを削除（切断中のみ許可）。Bond済みの相手は保存鍵を再利用し
  //     passkeyは再表示されないため、2回目のPairingを見るには両側で消す。
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
