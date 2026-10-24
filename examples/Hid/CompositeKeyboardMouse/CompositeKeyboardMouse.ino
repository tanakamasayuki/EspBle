// en: One composite HID Service with keyboard and mouse reports
// ja: keyboardとmouseを1つのHID Serviceへ複合
#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  // en: Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
  // ja: スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
  ble.hidKeyboard().configure();
  ble.hidMouse().configure();
  EspBleConfig config;
  config.deviceName = "EspBle Composite HID";
  config.security.enabled = true;
  config.security.bonding = true;
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!ble.begin(config)) return;
  // en: Handle link loss here; state from the old connection must not be assumed valid.
  // ja: ここで切断に対応します。古い接続の状態を有効なままと考えないようにします。
  ble.onDisconnected([](const EspBleConnection &) { ble.advertising().start(); });
  ble.advertising().setName(config.deviceName);
  ble.advertising().start();
  Serial.println("Send 'h' to type hello, 'm' to move.");
}

void loop()
{
  if (Serial.available())
  {
    const char command = Serial.read();
    if (command == 'h') ble.hidKeyboard().write("hello");
    // en: Mouse coordinates here are relative deltas, not absolute screen positions.
    // ja: ここでのマウス座標は相対移動量で、画面上の絶対位置ではありません。
    else if (command == 'm') ble.hidMouse().move(10, 10);
  }
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
