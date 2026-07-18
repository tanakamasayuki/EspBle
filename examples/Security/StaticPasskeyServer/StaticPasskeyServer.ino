// en: StaticPasskeyServer - a GATT server requiring MITM-authenticated pairing with a
//     static 6-digit passkey. This board is the display side (DisplayOnly): it prints
//     the passkey and the connecting central types it. The characteristic requires an
//     authenticated link, so Just Works pairing cannot access it.
// ja: StaticPasskeyServer - 静的6桁passkeyによるMITM認証Pairingを要求するGATT Server。
//     ボードは表示側（DisplayOnly）で、passkeyをSerialへ表示する。接続するCentralがそれを入力する。
//     CharacteristicはMITM認証済みlinkを要求するため、Just Works Pairingではアクセスできない。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "9f78d810-802e-43e7-9003-706173736b79";
static constexpr const char *CHARACTERISTIC_UUID = "9f78d811-802e-43e7-9003-706173736b79";

// en: Example-only fixed value. Production devices should provision a unique passkey.
// ja: example用の固定値。製品ではデバイスごとに安全にprovisioningすること。
static constexpr uint32_t STATIC_PASSKEY = 438209;

EspBle ble;

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
  gattServer.addService(SERVICE_UUID);
  gattServer.addCharacteristic(SERVICE_UUID, CHARACTERISTIC_UUID, valueConfig);
  gattServer.setValue(SERVICE_UUID, CHARACTERISTIC_UUID, String("MITM protected value"));

  EspBleConfig config;
  config.deviceName = "EspBle Passkey";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true; // en: require MITM protection (passkey auth) / ja: MITM保護（passkey認証）を要求
  // en: display side / ja: 表示側
  config.security.ioCapability = EspBleSecurityIoCapability::DisplayOnly;
  config.security.staticPasskeyEnabled = true;
  config.security.staticPasskey = STATIC_PASSKEY;
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
  ble.update();
  delay(1);
}
