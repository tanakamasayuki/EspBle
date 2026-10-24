// en: Vendor Input reception and Output / Feature writes
// ja: Vendor Input受信とOutput / Feature書込み
#include <EspBle.h>

EspBle ble;
EspBleConnectionId connectionId = 0;

void setup()
{
  Serial.begin(115200);

  ble.hidHost().onDiscovered([](const EspBleHidKeyboardHostDiscovery &result) {
    Serial.printf("HID discovery: %s\n", result.success ? "ready" : result.detail.c_str());
  });
  ble.hidHost().onVendorInput([](const EspBleHidVendorInputEvent &event) {
    Serial.printf("Vendor Input report=%u length=%u data=", event.reportId,
      static_cast<unsigned>(event.rawLength));
    for (size_t index = 0; index < event.rawLength; ++index)
      Serial.printf("%s%02x", index == 0 ? "" : " ", event.rawData[index]);
    Serial.println();
  });

  EspBleConfig config;
  config.deviceName = "EspBle Vendor Host";
  config.preferredMtu = 100;
  config.security.enabled = true;
  config.security.bonding = true;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }
  // en: Connection is established here; subsequent operations use this peer/session state.
  // ja: ここで接続成立を確認し、この相手／セッションの情報で後続処理を行います。
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
  });
  // en: Read the security result and encrypted/authenticated/bonded flags; these describe different properties and must not be treated as interchangeable.
  // ja: セキュリティ結果とencrypted・authenticated・bondedを確認します。これらは別の性質なので、同じ意味として扱わないようにします。
  ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
    if (event.success) ble.hidHost().discover(event.connection.id);
  });
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &) { connectionId = 0; });
  // en: Match a suitable peer before connecting; discovery alone does not mean a connection exists.
  // ja: 条件に合う相手を選んで接続します。発見しただけでは接続は成立していません。
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionId != 0 || !result.advertisesService("1812")) return;
    ble.scanner().stop();
    ble.connect(result);
  });
  ble.scanner().start();
  Serial.println("Send 'o' for Output or 'f' for Feature after discovery.");
}

void loop()
{
  if (Serial.available() > 0 && connectionId != 0)
  {
    const char command = Serial.read();
    if (command == 'o')
    {
      const uint8_t report[] = {'O', 'U', 'T', 3, 4, 5, 6, 7};
      Serial.printf("Output: %s\n", ble.hidHost().sendVendorOutput(
        connectionId, report, sizeof(report)) ? "sent" : "failed");
    }
    else if (command == 'f')
    {
      const uint8_t report[] = {'F', 'E', 'A', 'T', 4, 5, 6, 7};
      Serial.printf("Feature: %s\n", ble.hidHost().sendVendorFeature(
        connectionId, report, sizeof(report)) ? "sent" : "failed");
    }
  }
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
