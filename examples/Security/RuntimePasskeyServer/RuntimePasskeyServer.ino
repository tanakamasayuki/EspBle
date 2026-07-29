// en: RuntimePasskeyServer - the display side of Passkey Entry with a passkey that
//     is generated per pairing instead of being fixed in the sketch. With no
//     static passkey configured, the stack draws a fresh 6-digit value and hands
//     it to onPasskeyDisplayed; a real product would show it on its screen.
// ja: RuntimePasskeyServer - Passkey Entryの表示側。passkeyをsketchに固定せず、
//     Pairingごとに生成する。静的passkeyを設定しなければスタックが6桁を新たに引き、
//     onPasskeyDisplayed へ渡す。実機なら画面に表示する場面。
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
  service = gattServer.addService(SERVICE_UUID);
  characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(characteristic, String("MITM protected value"));

  EspBleConfig config;
  config.deviceName = "EspBle Runtime Passkey";
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
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "Security %s: encrypted=%u authenticated=%u bonded=%u\n",
      event.success ? "established" : "failed",
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0);
  });
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

  ble.update();
  delay(1);
}
