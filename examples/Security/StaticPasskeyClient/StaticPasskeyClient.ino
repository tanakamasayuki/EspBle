// en: StaticPasskeyClient - central-side counterpart of StaticPasskeyServer: the keyboard
//     side (KeyboardOnly) that "types" the passkey. This trial API passes a preconfigured
//     passkey to the stack instead of waiting for runtime input. After MITM-authenticated
//     pairing it reads the protected characteristic.
// ja: StaticPasskeyClient - StaticPasskeyServerのCentral側。passkeyを「入力する」側
//     （KeyboardOnly）。現在の試行APIは実行時入力を待つ代わりに事前設定passkeyをスタックへ渡す。
//     MITM認証Pairing完了後、保護されたCharacteristicをReadする。
#include <EspBle.h>

static constexpr const char *SERVICE_UUID = "9f78d810-802e-43e7-9003-706173736b79";
static constexpr const char *CHARACTERISTIC_UUID = "9f78d811-802e-43e7-9003-706173736b79";

// en: Must match the passkey displayed by StaticPasskeyServer.
// ja: StaticPasskeyServerが表示するpasskeyと一致させること。
static constexpr uint32_t STATIC_PASSKEY = 438209;

EspBle ble;
bool connectionRequested = false;

void setup()
{
  Serial.begin(115200);

  EspBleConfig config;
  config.deviceName = "EspBle Passkey Client";
  config.security.enabled = true;
  config.security.bonding = true;
  config.security.mitm = true;
  // en: this side "types" the passkey / ja: passkeyを「入力する」側
  config.security.ioCapability = EspBleSecurityIoCapability::KeyboardOnly;
  config.security.staticPasskeyEnabled = true;
  config.security.staticPasskey = STATIC_PASSKEY;
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  ble.onConnected([](const EspBleConnection &connection) {
    // en: Start pairing explicitly; completion arrives via onSecurityChanged().
    // ja: 明示的にPairingを開始する。完了は onSecurityChanged() で届く。
    if (!ble.requestSecurity(connection.id))
    {
      Serial.printf("Security request failed: %s\n", ble.lastErrorDetail().c_str());
    }
  });
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    Serial.printf(
      "Security %s: encrypted=%u authenticated=%u bonded=%u\n",
      event.success ? "established" : "failed",
      event.connection.encrypted ? 1 : 0,
      event.connection.authenticated ? 1 : 0,
      event.connection.bonded ? 1 : 0);
    if (event.success)
    {
      // en: The characteristic requires an authenticated link, so this only
      //     succeeds after MITM pairing completed.
      // ja: CharacteristicはMITM認証済みlinkを要求するため、MITM Pairing完了後にのみ成功する。
      ble.discoverCharacteristic(event.connection.id, SERVICE_UUID, CHARACTERISTIC_UUID);
    }
  });
  ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
    if (result.success)
    {
      ble.readCharacteristic(result.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID);
    }
  });
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
  ble.onDisconnected([](const EspBleConnection &) {
    connectionRequested = false;
  });
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
  // en: On 'c', delete all bonds (allowed only while disconnected).
  // ja: 'c' で全Bondを削除（切断中のみ許可）。
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
