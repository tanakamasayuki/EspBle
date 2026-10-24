// en: MITM-authenticated characteristic with a static passkey (display side)
// ja: 静的passkeyによるMITM認証Characteristic（表示側）
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "9f78d810-802e-43e7-9003-706173736b79";
static constexpr const char *CHARACTERISTIC_UUID = "9f78d811-802e-43e7-9003-706173736b79";

// en: Example-only fixed value. Production devices should provision a unique passkey.
// ja: example用の固定値。製品ではデバイスごとに安全にprovisioningすること。
static constexpr uint32_t STATIC_PASSKEY = 438209;

EspBle ble;

EspBleGattService service;
EspBleGattCharacteristic characteristic;
void setup()
{
  Serial.begin(115200);

  EspBleGattCharacteristicConfig valueConfig;
  valueConfig.readable = true;
  valueConfig.writable = true;
  // en: require a MITM-authenticated link / ja: MITM認証済みlinkを要求
  valueConfig.authenticatedRead = true;
  valueConfig.authenticatedWrite = true;

  auto &gattServer = ble.gattServer();
  // en: Register the local GATT service and its characteristics before ble.begin().
  // ja: ble.begin()の前に、ローカルのGATT ServiceとCharacteristicを登録します。
  service = gattServer.addService(SERVICE_UUID);
  characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(characteristic, String("MITM protected value"));

  EspBleConfig config;
  config.deviceName = "EspBle Passkey";
  // en: Pairing secures the link; bonding saves trust for later connections (see README).
  // ja: Pairingでリンクを保護し、Bondingで次回の接続用に信頼情報を保存します（README参照）。
  config.security.enabled = true;
  config.security.bonding = true;
  // en: require MITM protection (passkey auth)
  // ja: MITM保護（passkey認証）を要求
  config.security.mitm = true;
  // en: display side / ja: 表示側
  config.security.ioCapability = EspBleSecurityIoCapability::DisplayOnly;
  config.security.staticPasskeyEnabled = true;
  config.security.staticPasskey = STATIC_PASSKEY;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: The passkey to display arrives when pairing starts (delivered in update() context).
  // ja: Pairing開始時に表示すべきpasskeyが届く（update() contextで配送）。
  ble.onPasskeyDisplayed([](const EspBlePasskeyDisplayed &event) {
    Serial.printf("Enter passkey %06u on the peer.\n", static_cast<unsigned>(event.passkey));
  });
  // en: On success authenticated=1 (MITM-authenticated).
  // ja: 成功時は authenticated=1（MITM認証済み）になる。
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
  advertising.setName("EspBle Passkey");
  advertising.addServiceUuid(SERVICE_UUID);
  advertising.start();
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
