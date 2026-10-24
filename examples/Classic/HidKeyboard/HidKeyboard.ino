// en: Keyboard and mouse through the same profile API the BLE examples use
// ja: BLE exampleと同じprofile APIによるkeyboard / mouse
#include <EspBleClassic.h>

EspBleClassic bluetooth;

void setup()
{
  Serial.begin(115200);

  // en: Profiles are configured before begin(): the composed Report Descriptor
  //     is part of the device record the Host stores when it pairs.
  // ja: profileはbegin()より前に設定する。合成したReport Descriptorは、Hostが
  //     pairing時に保存するdevice recordの一部になるため。
  EspBleClassicHidProfileConfig hidConfig;
  hidConfig.name = "EspBle Classic Keyboard";
  // en: Compose the HID profile before stack initialization. If you change the report format, keep its descriptor and payload layout consistent.
  // ja: スタック初期化前にHIDプロファイルを構成します。Report形式を変更するときはDescriptorとデータの並びを一致させます。
  bluetooth.hidKeyboard().configure(hidConfig);
  bluetooth.hidMouse().configure(hidConfig);

  bluetooth.hidKeyboard().onOutputReport(
    [](const EspBleClassicHidKeyboardLeds &leds) {
      Serial.printf("LEDs from %s: caps=%u num=%u\n",
        leds.peerAddress.c_str(), leds.capsLock ? 1 : 0, leds.numLock ? 1 : 0);
    });

  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic Keyboard";
  // en: Check initialization before starting dependent operations.
  // ja: 初期化の成功を確認してから後続の処理を始めます。
  if (!bluetooth.begin(config))
  {
    Serial.printf(
      "Classic init failed: %s: %s\n",
      bluetooth.lastErrorName(), bluetooth.lastErrorDetail().c_str());
    return;
  }
  Serial.println("Pair from the host, then press the button on GPIO 0");
  // en: The internal pull-up makes the button active-low: pressed means GPIO 0 is connected to GND.
  // ja: 内部プルアップを使うので、GPIO 0をGNDへ落としたLOWがボタン押下になります。
  pinMode(0, INPUT_PULLUP);
}

void loop()
{
  // en: Dispatch queued events regularly; keep loop responsive while waiting for user input.
  // ja: イベントを定期的に配送します。入力待ちの間もloopを止めないようにします。
  bluetooth.update();

  static bool pressed;
  // en: Remember the previous state so holding the button does not type on every loop; this is not debouncing.
  // ja: 前回の状態を記憶して長押しで毎loop入力しないようにします。接点のデバウンス処理ではありません。
  const bool down = digitalRead(0) == LOW;
  if (down && !pressed && bluetooth.hidKeyboard().ready())
  {
    // en: Layout-aware text entry, the same call as on BLE.
    // ja: layoutを考慮した文字入力。BLEと同じ呼び出し。
    bluetooth.hidKeyboard().write("hello");
    // en: Mouse coordinates here are relative deltas, not absolute screen positions.
    // ja: ここでのマウス座標は相対移動量で、画面上の絶対位置ではありません。
    bluetooth.hidMouse().move(10, 0);
  }
  pressed = down;
  delay(10);
}
