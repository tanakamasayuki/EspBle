// en: Five-button relative mouse
// ja: 5ボタン相対mouse
#include <EspBle.h>

EspBle ble;

void setup()
{
  Serial.begin(115200);
  // en: Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
  // ja: スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
  ble.hidMouse().configure();
  EspBleConfig config;
  config.deviceName = "EspBle Mouse";
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
  Serial.println("Send 'm' to move, 'c' to click.");
}

void loop()
{
  if (Serial.available())
  {
    const char command = Serial.read();
    // en: Mouse coordinates here are relative deltas, not absolute screen positions.
    // ja: ここでのマウス座標は相対移動量で、画面上の絶対位置ではありません。
    if (command == 'm') ble.hidMouse().move(12, -8);
    else if (command == 'c') ble.hidMouse().click(ESP_BLE_HID_MOUSE_LEFT);
  }
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  ble.update();
  delay(1);
}
