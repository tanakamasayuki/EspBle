// en: N-key rollover keyboard (29-byte bitmap report)
// ja: N-key rollover keyboard（29-byte bitmap report）
#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);

  auto &keyboard = ble.hidKeyboard();
  keyboard.enableNkro();
  // en: Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
  // ja: スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
  keyboard.configure();

  EspBleConfig config;
  config.deviceName = "EspBle NKRO Keyboard";
  config.preferredMtu = 64; // A 29-byte NKRO Input Report needs MTU >= 32.
  config.security.enabled = true;
  config.security.bonding = true;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config))
  {
    Serial.printf("BLE initialization failed: %s\n", ble.lastErrorDetail().c_str());
    return;
  }

  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName("EspBle NKRO Keyboard");
  ble.advertising().start();
  Serial.println("Send 'n' for eight simultaneous keys, 'r' to release all.");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    auto &keyboard = ble.hidKeyboard();
    // en: ready() is false until a host is connected, encrypted, and subscribed to the Input Report. Sending before that fails with InvalidState, and a host that has not arrived yet is a normal state rather than an error, so it does not touch lastError().
    // ja: ready()はHostの接続・暗号化・Input Report購読がそろうまでfalseです。相手がまだいないのは通常状態なので、ready()はlastError()を変更しません。
    if (!keyboard.ready())
    {
      Serial.println("No subscribed HID Host yet.");
    }
    else if (command == 'n')
    {
      // en: sendReport(EspBleHidKeyboardReport) carries keys[6] and still expresses only six usages with NKRO enabled, so eight keys go out as one whole-state report. pressUsage() could hold eight keys too, but each key change would be its own notification and the chord would be paced by the connection interval.
      // ja: 通常のsendReportはkeys[6]なので、NKROでも6キーまでです。8キーの同時押しはNKROの全状態Reportにまとめ、個々の変更通知で接続間隔に分散しないようにします。
      EspBleHidKeyboardNkroReport report;
      const uint8_t usages[] = {0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
      for (uint8_t usage : usages) report.press(usage);
      // en: Keep the report ID and payload layout consistent with the configured descriptor.
      // ja: Report IDとデータの並びを、設定したDescriptorに一致させます。
      keyboard.sendReport(report);
    }
    else if (command == 'r')
    {
      // en: Send a released state so the host does not keep keys or buttons logically held.
      // ja: 解除状態を送信し、Host側でキーやボタンが押されたままにならないようにします。
      keyboard.releaseAll();
    }
  }
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
